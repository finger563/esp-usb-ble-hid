#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <NimBLEDevice.h>

#include "battery_service.hpp"
#include "ble_appearances.hpp"
#include "device_info_service.hpp"
#include "generic_access_service.hpp"
#include "hid_service.hpp"
#include "timer.hpp"

typedef NimBLERemoteCharacteristic::notify_callback notify_callback_t;

void init_ble(const std::string &device_name);
void start_ble_reconnection_thread(notify_callback_t callback);
void start_ble_pairing_thread(notify_callback_t callback);
bool is_ble_subscribed();
/// Whether a scan (pairing or reconnection) is currently running.
bool is_ble_scanning();
/// Whether the current / last scan is a pairing scan (accepts new devices).
bool is_ble_pairing();
/// Number of bonded (paired) controllers.
uint8_t ble_bond_count();
/// One bonded (paired) controller.
struct BleBond {
  std::array<uint8_t, 6> address{}; ///< identity address bytes (NimBLE order)
  uint8_t address_type{0};          ///< BLE address type
  bool connected{false};            ///< currently connected
};
/// The bonded (paired) controllers.
std::vector<BleBond> ble_bonds();
/// Called (from the BLE host task or the scan timer) when the controller link
/// goes down after it had been subscribed -- the app must neutralize the
/// inputs it forwards, since no further notifications will arrive.
using disconnect_callback_t = std::function<void()>;
void ble_set_disconnect_callback(disconnect_callback_t callback);
/// The HID report id a subscribed characteristic carries (from its Report
/// Reference descriptor); nullopt for an unknown characteristic.
std::optional<uint8_t> ble_report_id_for(const NimBLERemoteCharacteristic *chr);
/// Link diagnostics: notifications received since the current subscription
/// began, and the age of the last one (UINT32_MAX if none yet).
uint32_t ble_notification_count();
uint32_t ble_ms_since_last_notification();
/// Where the controller link currently is. The values are the ones reported
/// to the console (device_config INFO), so they must not be renumbered.
enum class BleLinkState : uint8_t {
  Idle = 1,        ///< not scanning and not connected (only before the first scan)
  Scanning = 2,    ///< scanning for a bonded (or, when pairing, any) controller
  Connecting = 3,  ///< a connection attempt is in flight
  Encrypting = 4,  ///< connected; waiting for the bond / link encryption
  Subscribing = 5, ///< encrypted; discovering + subscribing to the HID input reports
  Subscribed = 6,  ///< subscribed; inputs are expected (see ble_notification_count)
};
BleLinkState ble_link_state();
/// A short note about the last link event or problem ("" if none), e.g. why the
/// last connection was dropped or which step is being retried.
std::string ble_link_detail();
/// Called (from the BLE scan-timer task) once a controller is connected and
/// subscribed, with its identity address and its name -- the GAP Device Name
/// (0x2A00) if readable, else the advertised name, else "".
using bond_name_callback_t = std::function<void(const std::array<uint8_t, 6> &address,
                                                uint8_t address_type, const std::string &name)>;
void ble_set_bond_name_callback(bond_name_callback_t callback);
/// Forget one bond (disconnecting it first if it is the connected controller).
/// Returns false if no such bond exists.
bool ble_forget_bond(const std::array<uint8_t, 6> &address, uint8_t address_type);
/// Disconnect any connected controller and forget every bond. Returns false if
/// the bond store could not be cleared (bonds may remain).
bool ble_clear_bonds();
std::string get_connected_client_serial_number();
