#pragma once

// USB side of the bridge, built on espp::UsbDevice. The controller it presents
// is the "USB controller" setting (device_config::UsbController), fixed for the
// boot since USB descriptors are fixed at enumeration:
//  - Switch Pro: a HID interface presenting the Nintendo Switch Pro controller
//    (espp::SwitchPro owns the handshake / report protocol), or
//  - Xbox 360: an X-Input interface (espp::UsbDevice's XInput function, 20-byte
//    reports pushed with usb_push_xinput()); the device keeps Microsoft's vendor
//    ID with its own product ID (CONFIG_DONGLE_USB_XBOX_PID) and advertises the
//    XUSB10 compatible ID so Windows binds its Xbox 360 driver to the interface
//    while the console interface below stays available,
//  - optionally a vendor (WebUSB) interface carrying the espp dispatcher stream
//    (device configuration, OTA, crash dumps -- see services.hpp),
//  - optionally a CDC-ACM interface carrying the log console.

#include <functional>
#include <memory>
#include <span>

#include "sdkconfig.h"

#include "device_config.hpp"
#include "dispatcher_worker.hpp"
#include "switch_pro.hpp"
#include "xinput.hpp"

#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
/// The vendor stream's dispatcher + worker (espp::DispatcherWorker): register
/// services on it (at any time; it is created on first use), and every module
/// handler -- and its replies -- runs on the worker task, never on TinyUSB's.
/// Only built with the vendor interface (a plain HID build has no such stream
/// and spends no task on it).
espp::DispatcherWorker &usb_dispatcher();

/// Called on the dispatcher worker when vendor bytes had to be dropped (the
/// parser has already been reset); services use it to abort an in-flight
/// transfer.
/// May be set from any task at any time (the worker takes a copy under a lock).
void usb_set_rx_overflow_callback(std::function<void()> callback);
#endif // CONFIG_DONGLE_USB_VENDOR_INTERFACE

/// Bring up the USB device as @p which. Switch Pro: the controller's
/// input report is streamed to the host from a dedicated sender task once the
/// host enables reports; update it with controller->update_input_report().
/// Xbox 360: push each new state with usb_push_xinput() (@p ctrl is unused).
bool start_usb(device_config::UsbController which, const std::shared_ptr<espp::SwitchPro> &ctrl);

/// What the device is on USB this boot (the setting the firmware booted with).
device_config::UsbController usb_personality();

/// Xbox 360 mode: send an X-Input input report (no-op in Switch Pro mode or
/// while the host is not ready). Safe from any task except the TinyUSB task.
bool usb_push_xinput(const espp::xinput::GamepadState &state);

/// Whether the USB host has configured (mounted) the device.
bool usb_is_mounted();

/// Whether the host takes input: Switch Pro mode, the host has completed the
/// handshake and enabled input reports (the streamed reports are consumed);
/// Xbox 360 mode, the X-Input interface is mounted and accepting reports.
bool usb_hid_ready();

/// Input reports accepted by the USB stack since boot (diagnostics).
uint32_t usb_hid_reports_sent();

/// Write a complete frame to the vendor interface (no-op if it is disabled).
/// Safe to call from any task except the TinyUSB task.
bool usb_write_vendor(std::span<const uint8_t> frame);
