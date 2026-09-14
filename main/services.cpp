#include "services.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "esp_system.h"

#include "coredump.hpp"
#include "coredump_service.hpp"
#include "detail/ota_stream_protocol.hpp"
#include "logger.hpp"
#include "nvs.hpp"
#include "ota.hpp"
#include "stream_frame.hpp"

#include "usb.hpp"

using namespace std::chrono_literals;

static espp::Logger logger({.tag = "Services", .level = espp::Logger::Verbosity::INFO});

// --- settings persistence (NVS) --------------------------------------------------

static constexpr const char *kNvsNamespace = "dongle";
static std::unique_ptr<espp::Nvs> nvs_storage;

static device_config::Settings load_settings() {
  device_config::Settings s; // defaults
  if (!nvs_storage)
    return s;
  std::error_code ec;
  // get_or_set_var() writes the default the first time so the namespace is
  // fully populated after the first boot
  nvs_storage->get_or_set_var(kNvsNamespace, "inv_ly", s.invert_left_y, s.invert_left_y, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "inv_ry", s.invert_right_y, s.invert_right_y, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "swap_ab", s.swap_ab, s.swap_ab, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "swap_xy", s.swap_xy, s.swap_xy, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "deadzone", s.deadzone_percent, s.deadzone_percent,
                              ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "led", s.led_brightness, s.led_brightness, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "ble_name", s.ble_name, s.ble_name, ec);
  if (ec)
    logger.warn("Could not read all settings from NVS: {}", ec.message());
  // never trust stored values blindly (an older firmware may have stored
  // something this one rejects)
  if (auto why = s.validate(); !why.empty()) {
    logger.warn("Stored settings invalid ({}); using defaults", why);
    s = device_config::Settings{};
  }
  return s;
}

static void save_settings(const device_config::Settings &s) {
  if (!nvs_storage)
    return;
  std::error_code ec;
  nvs_storage->set_var(kNvsNamespace, "inv_ly", s.invert_left_y, ec);
  nvs_storage->set_var(kNvsNamespace, "inv_ry", s.invert_right_y, ec);
  nvs_storage->set_var(kNvsNamespace, "swap_ab", s.swap_ab, ec);
  nvs_storage->set_var(kNvsNamespace, "swap_xy", s.swap_xy, ec);
  nvs_storage->set_var(kNvsNamespace, "deadzone", s.deadzone_percent, ec);
  nvs_storage->set_var(kNvsNamespace, "led", s.led_brightness, ec);
  nvs_storage->set_var(kNvsNamespace, "ble_name", s.ble_name, ec);
  if (ec)
    logger.error("Could not save settings to NVS: {}", ec.message());
  else
    logger.info("Settings saved");
}

// --- module instances --------------------------------------------------------------

static std::unique_ptr<espp::Ota> ota;
static std::unique_ptr<espp::CoreDump> core_dump;
static std::unique_ptr<espp::CoreDumpService> coredump_service;
static std::unique_ptr<DeviceConfig> device_config_module;
static std::string crash_report;

static void send(std::span<const uint8_t> frame) { usb_write_vendor(frame); }

// --- OTA (module 0) -------------------------------------------------------------------
//
// Ported from the espp ota example: the device only handles requests, replies
// OK/ERROR per frame (one request in flight), and restarts shortly after a
// successful FINISH. Rollback is host-driven: after an update the new image
// boots PENDING_VERIFY and the host confirms it (MARK_VALID) once it has seen
// the device is healthy; otherwise the bootloader rolls back on the next reset.

namespace ota_proto = espp::detail::ota_stream;
static bool ota_session_active = false;

static void ota_reply_error(const std::error_code &err, const std::string &context) {
  send(ota_proto::make_error(static_cast<uint32_t>(err.value()), context + ": " + err.message()));
}

static void ota_handle_frame(const ota_proto::Frame &frame) {
  if (frame.is_reply())
    return;
  std::error_code ec;
  switch (static_cast<ota_proto::MessageType>(frame.type)) {
  case ota_proto::MessageType::Begin: {
    const auto image_size = ota_proto::parse_u32_payload(frame);
    if (!image_size.has_value()) {
      ota_reply_error(std::make_error_code(std::errc::invalid_argument), "malformed BEGIN");
      break;
    }
    if (ota->begin(*image_size, ec)) {
      ota_session_active = true;
      send(ota_proto::make_ok(0));
    } else {
      ota_reply_error(ec, "begin failed");
    }
    break;
  }
  case ota_proto::MessageType::Data:
    if (!ota_session_active) {
      ota_reply_error(std::make_error_code(std::errc::operation_not_permitted),
                      "no update session (send BEGIN first)");
      break;
    }
    if (ota->write(frame.payload, ec)) {
      send(ota_proto::make_ok(static_cast<uint32_t>(ota->bytes_written())));
    } else {
      ota_session_active = false; // write() aborted the session on failure
      ota_reply_error(ec, "write failed");
    }
    break;
  case ota_proto::MessageType::Finish: {
    if (!ota_session_active) {
      ota_reply_error(std::make_error_code(std::errc::operation_not_permitted),
                      "no update session (send BEGIN first)");
      break;
    }
    const auto written = static_cast<uint32_t>(ota->bytes_written());
    ota_session_active = false; // finish() ends the session in all outcomes
    if (ota->finish(ec)) {
      send(ota_proto::make_ok(written));
      // reply first, then restart into the new image
      std::thread([]() {
        std::this_thread::sleep_for(750ms);
        ota->restart();
      }).detach();
    } else {
      ota_reply_error(ec, "finish (validate/activate) failed");
    }
    break;
  }
  case ota_proto::MessageType::Abort: {
    if (!ota_session_active) {
      ota_reply_error(std::make_error_code(std::errc::operation_not_permitted),
                      "no update session to abort");
      break;
    }
    const auto written = static_cast<uint32_t>(ota->bytes_written());
    ota_session_active = false;
    if (ota->abort(ec))
      send(ota_proto::make_ok(written));
    else
      ota_reply_error(ec, "abort failed");
    break;
  }
  case ota_proto::MessageType::GetStatus: {
    uint8_t flags = 0;
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    flags |= ota_proto::kStatusRollbackSupported;
    if (ota->is_pending_verify())
      flags |= ota_proto::kStatusPendingVerify;
#endif
    const auto desc = ota->running_app_description();
    send(ota_proto::make_status(flags, desc.version, desc.project_name));
    break;
  }
  case ota_proto::MessageType::MarkValid:
    if (ota->mark_app_valid(ec))
      send(ota_proto::make_ok(0));
    else
      ota_reply_error(ec, "mark valid failed");
    break;
  case ota_proto::MessageType::MarkInvalid:
    // does not return on success (the device reboots into the previous image);
    // only a failure produces a reply
    ota->mark_app_invalid_and_rollback(ec);
    ota_reply_error(ec, "rollback failed");
    break;
  default:
    ota_reply_error(std::make_error_code(std::errc::not_supported), "unknown message type");
    break;
  }
}

static void on_rx_overflow() {
  // bytes were dropped: an in-flight image is unusable
  if (ota_session_active) {
    std::error_code ec;
    ota->abort(ec);
    ota_session_active = false;
  }
  send(ota_proto::make_error(
      static_cast<uint32_t>(std::make_error_code(std::errc::no_buffer_space).value()),
      "RX overflow: frames dropped; wait for OK replies between frames and retry"));
}

// --- public API -----------------------------------------------------------------------

void services_init(espp::Dispatcher &dispatcher, const ServicesCallbacks &callbacks) {
  // NVS (settings + BLE bonds live here)
  nvs_storage = std::make_unique<espp::Nvs>();
  std::error_code ec;
  nvs_storage->init(ec);
  if (ec)
    logger.error("NVS init failed: {}", ec.message());
  const auto settings = load_settings();

  // --- OTA ---
  ota = std::make_unique<espp::Ota>(
      espp::Ota::Config{.reject_same_version = false,
                        .progress_callback =
                            [](size_t written, size_t total) {
                              if (total > 0 && (written % (64 * 1024)) < 1024)
                                logger.info("OTA progress: {} / {} bytes", written, total);
                            },
                        .log_level = espp::Logger::Verbosity::INFO});
  const auto running = ota->running_app_description();
  logger.info("Running '{}' {} (built {} {}) from '{}'; next update -> '{}'", running.project_name,
              running.version, running.date, running.time, ota->running_partition_label(),
              ota->update_partition_label());
  if (ota->is_pending_verify())
    logger.warn("This image is PENDING VERIFY (first boot after an update): confirm it from the "
                "dongle console, or it rolls back on the next reset");
  dispatcher.register_module(
      ota_proto::kModule, [](const ota_proto::Frame &f) { ota_handle_frame(f); },
      {.name = "OTA", .app = "dongle_console.html", .description = "Firmware update over USB"});

  // --- crash dumps ---
  core_dump = std::make_unique<espp::CoreDump>();
  crash_report = core_dump->format_report();
  if (core_dump->has_core_dump())
    logger.warn("A crash core dump is stored:\n{}", crash_report);
  coredump_service = std::make_unique<espp::CoreDumpService>(
      *core_dump,
      espp::CoreDumpService::Config{.send = send, .log_level = espp::Logger::Verbosity::INFO});
  dispatcher.register_module(espp::CoreDumpService::kModule,
                             [](const espp::stream_frame::Frame &f) {
                               if (!f.is_reply())
                                 coredump_service->handle_frame(f.type, f.payload);
                             },
                             {.name = "Core Dump",
                              .app = "dongle_console.html",
                              .description = "Inspect the last crash core dump"});

  // --- device configuration ---
  device_config_module = std::make_unique<DeviceConfig>(DeviceConfig::Config{
      .send = send,
      .initial = settings,
      .on_settings_changed =
          [on_changed = callbacks.on_settings_changed](const device_config::Settings &s) {
            save_settings(s);
            if (on_changed)
              on_changed(s);
          },
      .info = callbacks.info,
      .on_action = callbacks.on_action,
      .bonds = callbacks.bonds,
      .forget_bond = callbacks.forget_bond,
      .log_level = espp::Logger::Verbosity::INFO});
  dispatcher.register_module(
      DeviceConfig::kModule,
      [](const espp::stream_frame::Frame &f) { device_config_module->handle(f); },
      DeviceConfig::module_info());

  // discovery: device name + firmware version, answered over the vendor stream
  dispatcher.set_device_info("ESP USB BLE HID", running.version);
  dispatcher.serve_discovery(send);
  usb_set_rx_overflow_callback(on_rx_overflow);
}

device_config::Settings services_settings() {
  return device_config_module ? device_config_module->settings() : device_config::Settings{};
}

std::string services_crash_report() { return crash_report; }
