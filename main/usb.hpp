#pragma once

#include <cstdint>
#include <vector>

#include "logger.hpp"

#include "gamepad_device.hpp"

extern "C" {
#include <class/hid/hid_device.h>
#include <tinyusb.h>
#include <tusb.h>
}

void start_usb_gamepad(const std::shared_ptr<GamepadDevice> &gamepad_device);
bool send_hid_report(uint8_t report_id, const std::vector<uint8_t> &report);
void stop_usb_gamepad();

// debugging

#include "gui.hpp"

void set_gui(std::shared_ptr<Gui> gui);
