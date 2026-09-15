#pragma once

// USB side of the bridge, built on espp::UsbDevice:
//  - a HID interface presenting the Nintendo Switch Pro controller
//    (espp::SwitchPro owns the handshake / report protocol),
//  - optionally a vendor (WebUSB) interface carrying the espp dispatcher stream
//    (device configuration, OTA, crash dumps -- see services.hpp),
//  - optionally a CDC-ACM interface carrying the log console.

#include <functional>
#include <memory>
#include <span>

#include "dispatcher.hpp"
#include "switch_pro.hpp"

/// The dispatcher for the vendor stream. Register modules on it BEFORE
/// start_usb() (registrations are also allowed later; the dispatcher defers
/// them safely).
espp::Dispatcher &usb_dispatcher();

/// Called from the RX worker when vendor bytes had to be dropped (the parser
/// has already been reset); services use it to abort an in-flight transfer.
/// May be set from any task at any time (the worker takes a copy under a lock).
void usb_set_rx_overflow_callback(std::function<void()> callback);

/// Bring up the USB device. The controller's input report is streamed to the
/// host from a dedicated sender task once the host enables reports; update it
/// with controller->update_input_report().
bool start_usb(const std::shared_ptr<espp::SwitchPro> &ctrl);

/// Whether the USB host has configured (mounted) the device.
bool usb_is_mounted();

/// Whether the host has completed the Switch Pro handshake and enabled input
/// reports (i.e. the streamed reports are actually being consumed).
bool usb_hid_ready();

/// Input reports accepted by the USB stack since boot (diagnostics).
uint32_t usb_hid_reports_sent();

/// Write a complete frame to the vendor interface (no-op if it is disabled).
/// Safe to call from any task except the TinyUSB task.
bool usb_write_vendor(std::span<const uint8_t> frame);
