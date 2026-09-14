#pragma once

#include <array>
#include <cstdint>
#include <functional>
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
/// Called (from the BLE scan-timer task) once a controller is connected and
/// subscribed, with its identity address and its name -- the GAP Device Name
/// (0x2A00) if readable, else the advertised name, else "".
using bond_name_callback_t = std::function<void(const std::array<uint8_t, 6> &address,
                                                uint8_t address_type, const std::string &name)>;
void ble_set_bond_name_callback(bond_name_callback_t callback);
/// Forget one bond (disconnecting it first if it is the connected controller).
/// Returns false if no such bond exists.
bool ble_forget_bond(const std::array<uint8_t, 6> &address, uint8_t address_type);
/// Disconnect any connected controller and forget every bond.
void ble_clear_bonds();
std::string get_connected_client_serial_number();
