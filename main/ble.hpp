#pragma once

#include <cstdint>

#include <NimBLEDevice.h>

#include "hid_service.hpp"
#include "timer.hpp"

typedef NimBLERemoteCharacteristic::notify_callback notify_callback_t;

void init_ble();
void start_ble_scan_thread(NimBLEUUID &service_uuid, NimBLEUUID &char_uuid,
                           notify_callback_t callback);
bool is_ble_subscribed();
