#include "ble.hpp"

/************* BLE Configuration ****************/

static uint32_t scanTimeMs = 5000; // scan time in milliseconds, 0 = scan forever
static std::unique_ptr<espp::Timer> scanTimer;
static bool subscribed = false;
static NimBLEUUID scan_for_service_uuid = NimBLEUUID("1812");

class ClientCallbacks : public NimBLEClientCallbacks {
  espp::Logger logger =
      espp::Logger({.tag = "BLE Client Callbacks", .level = espp::Logger::Verbosity::INFO});
  void onConnect(NimBLEClient *pClient) override {
    logger.info("connected to: {}", pClient->getPeerAddress().toString());
    static constexpr bool async = true;
    pClient->secureConnection(async);
  }

  void onDisconnect(NimBLEClient *pClient, int reason) override {
    logger.info("{} Disconnected, reason = {} - Starting scan",
                pClient->getPeerAddress().toString(), reason);
    NimBLEDevice::getScan()->start(scanTimeMs);
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
      NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->updateConnParams(12, 12, 0, 400);
    }
  }
} clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  espp::Logger logger =
      espp::Logger({.tag = "BLE Scan Callbacks", .level = espp::Logger::Verbosity::INFO});
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    logger.info("Advertised Device found: {}", advertisedDevice->toString());
    if (advertisedDevice->isAdvertisingService(scan_for_service_uuid)) {
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

      // set the connection parameters before we connect
      pClient->setConnectionParams(12, 12, 0, 400);
      // and set our callbacks
      pClient->setClientCallbacks(&clientCallbacks, false);
      if (!pClient->connect(true, true,
                            false)) { // delete attributes, async connect, no MTU exchange
        NimBLEDevice::deleteClient(pClient);
        logger.error("Failed to connect");
        return;
      }
    }
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    printf("Scan Ended\n");
    NimBLEDevice::getScan()->start(scanTimeMs);
  }
} scanCallbacks;

void init_ble() {
  NimBLEDevice::init("ESP-USB-BLE-HID");

  // // and some i/o config
  // auto io_capabilities = BLE_HS_IO_NO_INPUT_OUTPUT;
  // NimBLEDevice::setSecurityIOCap(io_capabilities);

  // // set security parameters
  // bool bonding = true;
  // bool mitm = false;
  // bool secure_connections = true;
  // NimBLEDevice::setSecurityAuth(bonding, mitm, secure_connections);
}

void start_ble_scan_thread(NimBLEUUID &service_uuid, NimBLEUUID &char_uuid,
                           notify_callback_t callback) {
  NimBLEScan *pScan = NimBLEDevice::getScan();

  // store the service uuid to look for
  scan_for_service_uuid = service_uuid;

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

  // now start a thread to register for notifications if connected or restart
  // scanning if not connected
  auto timer_fn = [service_uuid, char_uuid, callback]() -> bool {
    if (subscribed) {
      return false; // don't stop the timer
    }

    auto pClients = NimBLEDevice::getConnectedClients();

    // if there are no clients, then ensure we're scanning and return.
    if (!pClients.size()) {
      if (!NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->start(scanTimeMs);
      }
      return false; // don't stop the timer
    }

    // try to subscribe to notifications for each connected client
    for (auto &pClient : pClients) {
      // subscribe to notifications for the service and characteristic
      auto pSvc = pClient->getService(service_uuid);
      if (!pSvc) {
        continue;
      }
      auto pChr = pSvc->getCharacteristic(char_uuid);
      if (!pChr) {
        continue;
      }
      if (pChr->canNotify()) {
        if (!pChr->subscribe(true, callback)) {
          pClient->disconnect();
        } else {
          subscribed = true;
        }
      } else if (pChr->canIndicate()) {
        // Send false as first argument to subscribe to indications instead of notifications
        if (!pChr->subscribe(false, callback)) {
          pClient->disconnect();
        } else {
          subscribed = true;
        }
      }
    }

    return false; // don't stop the timer
  };
  using namespace std::chrono_literals;
  scanTimer = std::make_unique<espp::Timer>(
      espp::Timer::Config{.name = "Scan Timer",
                          .period = 100ms,
                          .callback = timer_fn,
                          .log_level = espp::Logger::Verbosity::INFO});
}

bool is_ble_subscribed() { return subscribed; }
