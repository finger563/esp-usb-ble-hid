#pragma once

#include <cstdint>

#include "logger.hpp"

#include "gamepad_device.hpp"

extern "C" {
#include <class/hid/hid_device.h>
#include <tinyusb.h>
#include <tusb.h>
}

void start_usb_gamepad(std::shared_ptr<GamepadDevice> &gamepad_device);
void stop_usb_gamepad();
