#include "ble.hpp"
#include "bsp.hpp"

#include "gaussian.hpp"

/************* BLE Configuration ****************/

static uint32_t scanTimeMs = 5000; // scan time in milliseconds, 0 = scan forever
static std::unique_ptr<espp::Timer> scanTimer;
static bool subscribed = false;

static NimBLEUUID hid_service_uuid(espp::HidService::SERVICE_UUID);
static NimBLEUUID hid_input_uuid(espp::HidService::REPORT_UUID);

static NimBLEUUID battery_service_uuid(espp::BatteryService::BATTERY_SERVICE_UUID);
static NimBLEUUID battery_level_uuid(espp::BatteryService::BATTERY_LEVEL_CHAR_UUID);

static bool is_pairing = true;
static notify_callback_t notify_callback = nullptr;

// LED configuration for BLE pairing / reconnecting
static constexpr float pairing_breathing_period = 1.0f;
static constexpr float reconnecting_breathing_period = 3.0f;

static float breathing_period = reconnecting_breathing_period;
static auto breathing_start = std::chrono::high_resolution_clock::now();
static espp::Gaussian gaussian({.gamma = 0.1f, .alpha = 1.0f, .beta = 0.5f});
static auto breathe = []() -> float {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(now - breathing_start).count();
  float t = std::fmod(elapsed, breathing_period) / breathing_period;
  return gaussian(t);
};
static auto led_callback = [](auto &m, auto &cv) -> bool {
  using namespace std::chrono_literals;
  static auto &bsp = Bsp::get();
  static espp::Rgb led_color(0.0f, 0.0f, 1.0f); // blue
  espp::Hsv hsv = led_color.hsv();
  hsv.v = breathe();
  bsp.led(hsv);
  std::unique_lock<std::mutex> lk(m);
  cv.wait_for(lk, 10ms);
  return false;
};
static auto led_task =
    espp::Task::make_unique({.callback = led_callback, .task_config = {.name = "breathe"}});

class ClientCallbacks : public NimBLEClientCallbacks {
  static constexpr uint16_t min_conn_interval = 12;    // 1.25ms units = 15ms
  static constexpr uint16_t max_conn_interval = 12;    // 1.25ms units = 15ms
  static constexpr uint16_t latency = 4;               // 4 packets at 15ms = 60ms
  static constexpr uint16_t supervision_timeout = 400; // 4s

  espp::Logger logger =
      espp::Logger({.tag = "BLE Client Callbacks", .level = espp::Logger::Verbosity::INFO});
  void onConnect(NimBLEClient *pClient) override {
    logger.info("connected to: {}", pClient->getPeerAddress().toString());
    static constexpr bool async = true;
    // set the connection parameters now that we've connected
    pClient->setConnectionParams(min_conn_interval, max_conn_interval, latency,
                                 supervision_timeout);
    // bond / secure the connection
    pClient->secureConnection(async);
    // stop the led task
    led_task->stop();
    static auto &bsp = Bsp::get();
    static espp::Rgb black(0.0f, 0.0f, 0.0f); // blue
    bsp.led(black);
  }

  void onDisconnect(NimBLEClient *pClient, int reason) override {
    logger.info("{} Disconnected, reason = {} - Starting scan",
                pClient->getPeerAddress().toString(), reason);
    // if we are not scanning, then start scanning
    if (!NimBLEDevice::getScan()->isScanning()) {
      start_ble_reconnection_thread(notify_callback);
    }
    subscribed = false;
  }

  void onAuthenticationComplete(NimBLEConnInfo &connInfo) override {
    if (!connInfo.isEncrypted()) {
      logger.error("Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in connInfo */
      NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
      return;
    } else {
      logger.info("Encryption successful!");
      // set the connection parameters
      NimBLEDevice::getClientByHandle(connInfo.getConnHandle())
          ->updateConnParams(min_conn_interval, max_conn_interval, latency, supervision_timeout);
    }
  }
};

static ClientCallbacks clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  espp::Logger logger =
      espp::Logger({.tag = "BLE Scan Callbacks", .level = espp::Logger::Verbosity::INFO});
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    logger.info("Advertised Device found: {}", advertisedDevice->toString());
    bool should_connect = false;
    bool is_pairable_device =
        advertisedDevice->isAdvertisingService(hid_service_uuid) ||
        advertisedDevice->getAppearance() == (uint16_t)espp::BleAppearance::GAMEPAD;
    if (is_pairing && is_pairable_device) {
      // if we're pairing, then simply connect to the first device that advertises
      // the HID service. The connection callback will try to bond to it.
      should_connect = true;
    } else if (!is_pairing && NimBLEDevice::isBonded(advertisedDevice->getAddress())) {
      // if we're not pairing, then we're reconnecting, so we need to check if
      // the device is bonded
      should_connect = true;
    }
    if (should_connect) {
      /** stop scan before connecting, since we use async connections and don't
          want to possibly try to connect to multiple devices. */
      NimBLEDevice::getScan()->stop();

      logger.info("Found Our Device");

      /** Async connections can be made directly in the scan callbacks */
      auto pClient = NimBLEDevice::getDisconnectedClient();
      if (!pClient) {
        pClient = NimBLEDevice::createClient(advertisedDevice->getAddress());
        if (!pClient) {
          logger.error("Failed to create client");
          return;
        }
      }

      // and set our callbacks
      pClient->setClientCallbacks(&clientCallbacks, false);
      static constexpr bool delete_on_disconnect = true;
      static constexpr bool delete_on_connect_fail = true;
      pClient->setSelfDelete(delete_on_disconnect, delete_on_connect_fail);
      if (!pClient->connect(true, true,
                            false)) { // delete attributes, async connect, no MTU exchange
        logger.error("Failed to connect");
        return;
      }
    }
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    printf("Scan Ended\n");
    start_ble_reconnection_thread(notify_callback);
  }
};

std::string get_connected_client_serial_number() {
  auto clients = NimBLEDevice::getConnectedClients();
  if (clients.size() == 0) {
    return "";
  }
  auto client = clients[0];
  // get the device info service
  auto svc = client->getService(espp::DeviceInfoService::SERVICE_UUID);
  if (!svc) {
    return "";
  }
  // get the serial number characteristic
  auto chr = svc->getCharacteristic(espp::DeviceInfoService::SERIAL_NUMBER_CHAR_UUID);
  // make sure we can read it
  if (!chr->canRead()) {
    return {};
  }
  // and read it
  auto value = chr->readValue();
  return value;
}

static ScanCallbacks scanCallbacks;

static bool timer_callback() {
  if (subscribed) {
    return false; // don't stop the timer
  }

  auto pClients = NimBLEDevice::getConnectedClients();

  // if there are no clients, then ensure we're scanning and return.
  if (!pClients.size()) {
    if (!NimBLEDevice::getScan()->isScanning()) {
      start_ble_reconnection_thread(notify_callback);
    }
    return false; // don't stop the timer
  }

  // try to subscribe to notifications for each connected client
  for (auto &pClient : pClients) {
    static constexpr bool refresh = true;
    // refresh services
    pClient->getServices(refresh);
    auto pSvc = pClient->getService(hid_service_uuid);
    if (pSvc) {
      // refresh chars
      pSvc->getCharacteristics(refresh);
      auto pChr = pSvc->getCharacteristic(hid_input_uuid);
      if (!pChr) {
        continue;
      }
      // subscribe to the characteristic
      if (!pChr->subscribe(pChr->canNotify(), notify_callback)) {
        pClient->disconnect();
      } else {
        subscribed = true;
      }
      if (subscribed) {
        // we were able to get the HID service and subscribe, so also subscribe
        // to the battery service if it exists.
        auto pBatterySvc = pClient->getService(battery_service_uuid);
        if (pBatterySvc) {
          pBatterySvc->getCharacteristics(refresh);
          auto pBatteryChr = pBatterySvc->getCharacteristic(battery_level_uuid);
          if (pBatteryChr) {
            // ignore success here since it's not high priority
            pBatteryChr->subscribe(pBatteryChr->canNotify(), notify_callback);
          }
        }
      }
    }
    if (!subscribed) {
      // if we could not subscribe, then delete the bond info for the client so
      // that we don't try to reconnect to them in the future.
      NimBLEDevice::deleteBond(pClient->getConnInfo().getIdAddress());
    }
  }

  return false; // don't stop the timer
}

void init_ble(const std::string &device_name) {
  NimBLEDevice::init(device_name);
  // NOTE: you must create a server if you want the GAP services to be available
  // and the device name to be readable by connected peers.
  static auto server_ = NimBLEDevice::createServer();
  server_->start();

  // // and some i/o config
  auto io_capabilities = BLE_HS_IO_NO_INPUT_OUTPUT;
  NimBLEDevice::setSecurityIOCap(io_capabilities);

  // // set security parameters
  bool bonding = true;
  bool mitm = false;
  bool secure_connections = true;
  NimBLEDevice::setSecurityAuth(bonding, mitm, secure_connections);
}

static void start_scan() {
  // if scanning, then stop
  if (NimBLEDevice::getScan()->isScanning()) {
    NimBLEDevice::getScan()->stop();
  }

  NimBLEScan *pScan = NimBLEDevice::getScan();

  // Set the callbacks to call when scan events occur, no duplicates
  pScan->setScanCallbacks(&scanCallbacks);

  // Set scan interval (how often) and window (how long) in milliseconds
  pScan->setInterval(100);
  pScan->setWindow(100);

  // Active scan will gather scan response data from advertisers
  // but will use more energy from both devices
  pScan->setActiveScan(true);

  // Start scanning for advertisers
  pScan->start(scanTimeMs);

  // if the led task is not running, set the led breathing start time to now
  if (!led_task->is_running()) {
    breathing_start = std::chrono::high_resolution_clock::now();
  }

  // now start the led task
  led_task->start();

  if (!scanTimer) {
    // now start a thread to register for notifications if connected or restart
    // scanning if not connected
    using namespace std::chrono_literals;
    scanTimer = std::make_unique<espp::Timer>(
        espp::Timer::Config{.name = "Scan Timer",
                            .period = 100ms,
                            .callback = timer_callback,
                            .log_level = espp::Logger::Verbosity::INFO});
  }
}

void start_ble_reconnection_thread(notify_callback_t callback) {
  // if there are no bonded devices, then instead call the pairing thread
  if (NimBLEDevice::getNumBonds() == 0) {
    start_ble_pairing_thread(callback);
    return;
  }
  // set pairing to false
  is_pairing = false;
  // save the callback
  notify_callback = callback;
  // set the breathing period
  breathing_period = reconnecting_breathing_period;
  // now start the scan
  start_scan();
}

void start_ble_pairing_thread(notify_callback_t callback) {
  // set pairing to true
  is_pairing = true;
  // save the callback
  notify_callback = callback;
  // set the breathing period
  breathing_period = pairing_breathing_period;
  // now start the scan
  start_scan();
}

bool is_ble_subscribed() { return subscribed; }
