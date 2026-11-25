#pragma once

// The dongle's "services" on the USB vendor stream (espp dispatcher modules):
//   module 0x00  OTA firmware update        (espp::OtaService)
//   module 0x04  crash-dump download         (espp::CoreDumpService)
//   module 0x07  system info / reboot        (espp::SystemService: identity, reboot,
//                                             reboot into the ROM bootloader)
//   module 0x08  heap + task monitor         (espp::MonitorService)
//   module 0x0B  gamepad button / axis maps  (espp::GamepadMapService: the default map +
//                                             rumble options, and one profile per
//                                             controller type, in a LittleFS file)
//   module 0x10  device configuration        (DeviceConfig, components/device_config)
// plus the persisted settings (NVS) the rest of the app reads.

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "dispatcher_worker.hpp"
#include "gamepad_map.hpp"
#include "gamepad_state.hpp"
#include "rumble_options.hpp"

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
  /// Called when the console changed the connected controller's button map.
  std::function<void(const espp::GamepadMap &)> on_gamepad_map_changed{nullptr};
  /// Called when the console changed the connected controller's rumble options
  /// (under the gamepad store's change mutex).
  std::function<void(const espp::RumbleOptions &)> on_gamepad_rumble_changed{nullptr};
  /// Play the rumble test pattern on the connected controller through these
  /// (unsaved) options, for the console's Test button; return at once, false
  /// with a short reason when it cannot. Unset: the console offers no preview.
  std::function<bool(const espp::RumbleOptions &, std::string &error)> on_gamepad_test_rumble{
      nullptr};
};

/// Initialize NVS, load the settings, and register the OTA / core-dump / system /
/// monitor / config services on @p link (the vendor stream's dispatcher worker); replies go out
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

/// A controller connected: returns the button map to apply to it (its
/// profile's, else the default map) and tells the console about it. Call
/// services_gamepad_rumble() right after, under the same lock the change
/// callbacks take.
espp::GamepadMap services_gamepad_connected(uint16_t vid, uint16_t pid, const std::string &name);
/// The rumble options the connected controller should use now (its profile's,
/// else the default ones; neutral without the service).
espp::RumbleOptions services_gamepad_rumble();
/// The controller disconnected.
void services_gamepad_disconnected();
/// Every input, before and after mapping, for the console's live view (cheap
/// while the console is not watching).
void services_gamepad_input(const espp::GamepadState &raw, const espp::GamepadState &mapped);
/// Whether the button maps are persisted (false on a dongle flashed before the
/// `littlefs` partition existed: the maps then last until the next reboot, and
/// the console offers a full update that adds the partition).
bool services_gamepad_storage_available();
