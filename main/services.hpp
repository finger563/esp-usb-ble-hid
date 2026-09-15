#pragma once

// The dongle's "services" on the USB vendor stream (espp dispatcher modules):
//   module 0x00  OTA firmware update        (espp::OtaService)
//   module 0x04  crash-dump download         (espp::CoreDumpService)
//   module 0x10  device configuration        (DeviceConfig, components/device_config)
// plus the persisted settings (NVS) the rest of the app reads.

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "dispatcher_worker.hpp"

#include "device_config.hpp"

struct ServicesCallbacks {
  /// Live status for the config module's GET_INFO.
  std::function<device_config::Info()> info{nullptr};
  /// Perform a config-module action (pairing / clear bonds / reboot).
  std::function<bool(device_config::Action, std::string &error)> on_action{nullptr};
  /// The paired controllers.
  std::function<std::vector<device_config::BondInfo>()> bonds{nullptr};
  /// Forget one paired controller.
  std::function<bool(const std::array<uint8_t, 6> &address, uint8_t address_type,
                     std::string &error)>
      forget_bond{nullptr};
  /// Called whenever the settings changed (already persisted); apply them.
  std::function<void(const device_config::Settings &)> on_settings_changed{nullptr};
};

/// Initialize NVS, load the settings, and register the OTA / core-dump / config
/// services on @p link (the vendor stream's dispatcher worker); replies go out
/// through the link's sender. With @p link null (a build without the vendor
/// interface) the settings, OTA engine and crash report are still set up, but
/// no service is instantiated -- there is no stream to answer on.
void services_init(espp::DispatcherWorker *link, const ServicesCallbacks &callbacks);

/// A copy of the current (persisted) settings.
device_config::Settings services_settings();

/// Paired-controller names, persisted in NVS keyed by the bond's address so the
/// console can show something better than a MAC. Names come from the BLE side
/// (GAP Device Name / advertised name) each time a controller connects.
std::string services_bond_name(const std::array<uint8_t, 6> &address);
void services_set_bond_name(const std::array<uint8_t, 6> &address, const std::string &name);
void services_forget_bond_name(const std::array<uint8_t, 6> &address);
void services_clear_bond_names();

/// The last crash report (empty if the previous boot was clean).
std::string services_crash_report();
