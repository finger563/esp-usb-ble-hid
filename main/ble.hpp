#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
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
/// Whether the connected controller has a writable HID output report with
/// this id (found while subscribing; forgotten on disconnect).
bool ble_has_output_report(uint8_t report_id);
/// Write a HID output report (e.g. rumble) to the connected controller. @p data
/// is the report payload, without the report id. Non-blocking: queued for the
/// next connection event (a write without response when the characteristic
/// takes one, else a write with response whose reply is not waited for), so it
/// is safe from any task. Returns false when no controller is subscribed, it has
/// no such report, or the stack refused the write (out of buffers, say); the
/// caller decides whether to retry.
bool ble_write_output_report(uint8_t report_id, std::span<const uint8_t> data);
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
/// Pauses the scan for the deletion (the host refuses to unpair while
/// scanning). Returns false, with @p error set, if no such bond exists or the
/// bond store could not delete it.
bool ble_forget_bond(const std::array<uint8_t, 6> &address, uint8_t address_type,
                     std::string &error);
/// Disconnect any connected controller and forget every bond, pausing the scan
/// for the deletion. Returns false, with @p error set, if the bond store could
/// not be cleared (bonds may remain).
bool ble_clear_bonds(std::string &error);
std::string get_connected_client_serial_number();

/// What the dongle reads from a controller when it connects, before it
/// subscribes to the input reports: enough to decode them and to pick the
/// controller's button map.
struct BleControllerInfo {
  std::vector<uint8_t> report_map; ///< HID Report Map (0x2A4B); empty if it could not be read
  uint16_t vid{0};                 ///< Device Information PnP ID vendor (0 = absent)
  uint16_t pid{0};                 ///< Device Information PnP ID product (0 = absent)
  std::string name;                ///< device name (0x2A00), else the advertised name
};
/// Called (from the BLE scan-timer task) with the controller's info right
/// before each attempt to subscribe to its input reports, so the reports that
/// follow can be decoded. Set once at startup, before scanning.
using controller_info_callback_t = std::function<void(const BleControllerInfo &info)>;
void ble_set_controller_info_callback(controller_info_callback_t callback);
