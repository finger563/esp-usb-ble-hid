#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "esp_timer.h"

#include "ble.hpp"
#include "bsp.hpp"
#include "status_led.hpp"

#include "gaussian.hpp"

/************* BLE Configuration ****************/

static uint32_t scanTimeMs = 5000; // scan time in milliseconds, 0 = scan forever
static std::unique_ptr<espp::Timer> scanTimer;
// read from other tasks (the USB RX worker's status queries), written by the
// BLE callbacks / scan timer: keep them atomic
static std::atomic<bool> subscribed{false};
// the link is encrypted (bonded / re-bonded); protected HID characteristics are
// only discoverable + subscribable after this
static std::atomic<bool> authenticated{false};
static std::atomic<int64_t> connected_at_us{0};
static std::atomic<uint8_t> subscribe_attempts{0};

// link diagnostics, exposed to the console (a "connected but no inputs" report
// is much easier to chase when it says whether notifications are arriving)
static std::atomic<uint32_t> notification_count{0};
static std::atomic<int64_t> last_notify_us{-1};

static NimBLEUUID hid_service_uuid(espp::HidService::SERVICE_UUID);
static NimBLEUUID hid_input_uuid(espp::HidService::REPORT_UUID);
static NimBLEUUID report_reference_uuid(espp::HidService::REPORT_DESCRIPTOR_UUID); // 0x2908

static NimBLEUUID battery_service_uuid(espp::BatteryService::BATTERY_SERVICE_UUID);
static NimBLEUUID battery_level_uuid(espp::BatteryService::BATTERY_LEVEL_CHAR_UUID);

static std::atomic<bool> is_pairing{true};
static notify_callback_t notify_callback = nullptr;
static disconnect_callback_t disconnect_callback = nullptr;

// Which HID report id each subscribed characteristic carries (from its Report
// Reference descriptor), so the app can route a notification by report id
// instead of assuming every notification is the gamepad input report. Cleared
// on disconnect (the characteristic objects die with the client).
static std::mutex report_map_mutex;
static std::unordered_map<const NimBLERemoteCharacteristic *, uint8_t> report_ids;

// The controller's name, reported to the app once it is connected + subscribed
// (see ble_set_bond_name_callback). The advertised name is captured when we
// decide to connect (scan task) and used as the fallback if the GAP Device Name
// cannot be read (scan-timer task), hence the mutex.
static bond_name_callback_t bond_name_callback = nullptr;
static std::mutex advertised_name_mutex;
static std::string advertised_name;

static NimBLEUUID generic_access_service_uuid(espp::GenericAccessService::SERVICE_UUID);
static NimBLEUUID device_name_uuid(espp::GenericAccessService::NAME_CHAR_UUID);

static espp::Logger ble_logger({.tag = "BLE", .level = espp::Logger::Verbosity::INFO});

// Read the connected controller's name: GAP Device Name, else the name it
// advertised, else "". Control characters are stripped and the result capped
// so it is safe to store and display.
static std::string read_controller_name(NimBLEClient *client) {
  std::string name;
  if (auto *gap = client->getService(generic_access_service_uuid)) {
    if (auto *chr = gap->getCharacteristic(device_name_uuid); chr && chr->canRead()) {
      name = chr->readValue();
    }
  }
  if (name.empty()) {
    std::lock_guard<std::mutex> lk(advertised_name_mutex);
    name = advertised_name;
  }
  std::string clean;
  for (const char c : name) {
    if (static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) < 0x7F)
      clean.push_back(c);
    if (clean.size() >= 31)
      break;
  }
  return clean;
}

// Forget everything about the current link. Called from the NimBLE host task
// (disconnect callback) and from the scan timer (missed disconnect); the app is
// told through the disconnect callback so it can neutralize its inputs.
static void reset_link_state(const char *why) {
  const bool was_subscribed = subscribed.exchange(false);
  authenticated = false;
  subscribe_attempts = 0;
  {
    std::lock_guard<std::mutex> lk(report_map_mutex);
    report_ids.clear();
  }
  if (was_subscribed) {
    ble_logger.info("controller link down ({}); {} notifications received", why,
                    notification_count.load());
    if (disconnect_callback)
      disconnect_callback();
  }
}

// Every subscribed characteristic notifies through here: count it, then hand
// it to the app.
static void on_notify(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t len, bool is_notify) {
  notification_count.fetch_add(1);
  last_notify_us.store(esp_timer_get_time());
  if (notify_callback)
    notify_callback(chr, data, len, is_notify);
}

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
  static espp::Rgb led_color(0.0f, 0.0f, 1.0f); // blue
  espp::Hsv hsv = led_color.hsv();
  hsv.v = breathe();
  set_led(hsv);
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
    authenticated = false;
    subscribe_attempts = 0;
    connected_at_us = esp_timer_get_time();
    static constexpr bool async = true;
    // set the connection parameters now that we've connected
    pClient->setConnectionParams(min_conn_interval, max_conn_interval, latency,
                                 supervision_timeout);
    // bond / secure the connection
    pClient->secureConnection(async);
    // stop the led task
    led_task->stop();
    static espp::Rgb black(0.0f, 0.0f, 0.0f);
    set_led(black);
  }

  void onDisconnect(NimBLEClient *pClient, int reason) override {
    logger.info("{} Disconnected, reason = {} - Starting scan",
                pClient->getPeerAddress().toString(), reason);
    // drop the link state (and neutralize the app's inputs) BEFORE scanning again
    reset_link_state("disconnected");
    // if we are not scanning, then start scanning
    if (!NimBLEDevice::getScan()->isScanning()) {
      start_ble_reconnection_thread(notify_callback);
    }
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
      // the protected HID characteristics can be discovered + subscribed now
      authenticated = true;
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
      {
        // remember what it called itself, for the paired-controller list
        std::lock_guard<std::mutex> lk(advertised_name_mutex);
        advertised_name = advertisedDevice->haveName() ? advertisedDevice->getName() : "";
      }

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
  if (!chr || !chr->canRead()) {
    return {};
  }
  // and read it
  auto value = chr->readValue();
  return value;
}

static ScanCallbacks scanCallbacks;

// Subscribe to every HID *input* report the controller notifies (a gamepad may
// expose several: e.g. the Xbox controller's gamepad report and a separate
// report for the Xbox button). The report id of each comes from its Report
// Reference descriptor (0x2908: [report_id][report_type], type 1 = input);
// without that descriptor the characteristic is assumed to be the main input
// report. Returns the number of characteristics subscribed.
static size_t subscribe_input_reports(NimBLEClient *client) {
  static constexpr bool refresh = true;
  client->getServices(refresh);
  auto *svc = client->getService(hid_service_uuid);
  if (!svc) {
    ble_logger.warn("HID service not found (yet)");
    return 0;
  }
  size_t count = 0;
  for (auto *chr : svc->getCharacteristics(refresh)) {
    if (chr->getUUID() != hid_input_uuid || !chr->canNotify())
      continue;
    uint8_t report_id = 1;
    if (auto *ref = chr->getDescriptor(report_reference_uuid)) {
      const auto value = ref->readValue();
      if (value.size() >= 2) {
        report_id = value.data()[0];
        const uint8_t report_type = value.data()[1];
        if (report_type != 1) // 1 = input; skip output / feature reports
          continue;
      }
    }
    if (!chr->subscribe(true, on_notify)) {
      ble_logger.warn("could not subscribe to input report {}", report_id);
      continue;
    }
    {
      std::lock_guard<std::mutex> lk(report_map_mutex);
      report_ids[chr] = report_id;
    }
    ble_logger.info("subscribed to HID input report id {} (handle {:#06x})", report_id,
                    chr->getHandle());
    ++count;
  }
  return count;
}

static bool timer_callback() {
  auto pClients = NimBLEDevice::getConnectedClients();

  if (subscribed) {
    // A disconnect callback can be missed (e.g. the client object went away):
    // never leave the link marked up when nothing is connected.
    if (pClients.empty())
      reset_link_state("no connected client");
    return false; // don't stop the timer
  }

  // if there are no clients, then ensure we're scanning and return.
  if (pClients.empty()) {
    if (!NimBLEDevice::getScan()->isScanning()) {
      start_ble_reconnection_thread(notify_callback);
    }
    return false; // don't stop the timer
  }

  auto *pClient = pClients.front();
  if (!pClient->isConnected())
    return false;

  // The HID input reports are protected: wait for the bond/encryption to
  // complete before discovering and subscribing (trying earlier used to fail
  // and, worse, delete the bond). Give the controller a bounded time to pair.
  static constexpr int64_t kAuthTimeoutUs = 15 * 1000 * 1000;
  if (!authenticated) {
    if (esp_timer_get_time() - connected_at_us.load() > kAuthTimeoutUs) {
      ble_logger.warn("controller did not complete encryption in time; disconnecting");
      pClient->disconnect();
    }
    return false;
  }

  // Discovery + subscribe takes a while; try every ~500 ms (this timer runs at
  // 100 ms) and bounded, then drop the connection so the scan can start over.
  // A failed attempt is NOT a reason to forget the bond: transient discovery
  // failures right after a reconnect are normal.
  static constexpr uint8_t kMaxSubscribeAttempts = 10;
  static uint8_t throttle = 0;
  if (++throttle % 5 != 1)
    return false;
  const uint8_t attempt = ++subscribe_attempts;
  if (attempt > kMaxSubscribeAttempts) {
    ble_logger.error("could not subscribe to the controller's HID reports after {} attempts; "
                     "disconnecting (bond kept)",
                     kMaxSubscribeAttempts);
    pClient->disconnect();
    return false;
  }
  if (subscribe_input_reports(pClient) == 0) {
    ble_logger.warn("no HID input report subscribed (attempt {}/{})", attempt,
                    kMaxSubscribeAttempts);
    return false;
  }
  notification_count = 0;
  last_notify_us = -1;
  subscribed = true;

  // we were able to get the HID service and subscribe, so also subscribe
  // to the battery service if it exists.
  if (auto *pBatterySvc = pClient->getService(battery_service_uuid)) {
    pBatterySvc->getCharacteristics(true);
    if (auto *pBatteryChr = pBatterySvc->getCharacteristic(battery_level_uuid)) {
      // ignore success here since it's not high priority
      pBatteryChr->subscribe(pBatteryChr->canNotify(), on_notify);
    }
  }
  // and report the controller's name for the paired-controller list
  if (bond_name_callback) {
    const NimBLEAddress id = pClient->getConnInfo().getIdAddress();
    std::array<uint8_t, 6> address{};
    std::copy(id.getVal(), id.getVal() + address.size(), address.begin());
    bond_name_callback(address, id.getType(), read_controller_name(pClient));
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

bool is_ble_subscribed() { return subscribed.load(); }

bool is_ble_scanning() { return NimBLEDevice::getScan()->isScanning(); }

bool is_ble_pairing() { return is_pairing.load(); }

uint8_t ble_bond_count() { return static_cast<uint8_t>(NimBLEDevice::getNumBonds()); }

void ble_set_bond_name_callback(bond_name_callback_t callback) {
  bond_name_callback = std::move(callback); // set once at startup, before scanning
}

void ble_set_disconnect_callback(disconnect_callback_t callback) {
  disconnect_callback = std::move(callback); // set once at startup, before scanning
}

std::optional<uint8_t> ble_report_id_for(const NimBLERemoteCharacteristic *chr) {
  std::lock_guard<std::mutex> lk(report_map_mutex);
  const auto it = report_ids.find(chr);
  if (it == report_ids.end())
    return std::nullopt;
  return it->second;
}

uint32_t ble_notification_count() { return notification_count.load(); }

uint32_t ble_ms_since_last_notification() {
  const int64_t last = last_notify_us.load();
  if (last < 0)
    return UINT32_MAX;
  const int64_t age_ms = (esp_timer_get_time() - last) / 1000;
  return age_ms > static_cast<int64_t>(UINT32_MAX) ? UINT32_MAX : static_cast<uint32_t>(age_ms);
}

// The identity addresses of the connected clients (a bond is keyed by the
// identity address, not the possibly-random connection address).
static std::vector<NimBLEAddress> connected_identity_addresses() {
  std::vector<NimBLEAddress> out;
  for (auto *client : NimBLEDevice::getConnectedClients()) {
    if (client->isConnected())
      out.push_back(client->getConnInfo().getIdAddress());
  }
  return out;
}

std::vector<BleBond> ble_bonds() {
  std::vector<BleBond> bonds;
  const auto connected = connected_identity_addresses();
  const int count = NimBLEDevice::getNumBonds();
  for (int i = 0; i < count; ++i) {
    const NimBLEAddress addr = NimBLEDevice::getBondedAddress(i);
    BleBond b;
    std::copy(addr.getVal(), addr.getVal() + b.address.size(), b.address.begin());
    b.address_type = addr.getType();
    b.connected = std::find(connected.begin(), connected.end(), addr) != connected.end();
    bonds.push_back(b);
  }
  return bonds;
}

bool ble_forget_bond(const std::array<uint8_t, 6> &address, uint8_t address_type) {
  const NimBLEAddress addr(address.data(), address_type);
  if (!NimBLEDevice::isBonded(addr))
    return false;
  // drop the live connection first if it is this controller (the client would
  // otherwise re-bond)
  for (auto *client : NimBLEDevice::getConnectedClients()) {
    if (client->isConnected() && client->getConnInfo().getIdAddress() == addr) {
      client->disconnect();
      reset_link_state("forgotten");
    }
  }
  return NimBLEDevice::deleteBond(addr);
}

bool ble_clear_bonds() {
  // drop the live connection first (its client would otherwise re-bond)
  for (auto *client : NimBLEDevice::getConnectedClients()) {
    client->disconnect();
  }
  reset_link_state("all bonds cleared");
  return NimBLEDevice::deleteAllBonds();
}
