#include "services.hpp"

#include <chrono>
#include <memory>
#include <mutex>

#include "esp_system.h"

#include "coredump.hpp"
#include "coredump_service.hpp"
#include "format.hpp"
#include "logger.hpp"
#include "nvs.hpp"
#include "ota.hpp"
#include "ota_service.hpp"

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
  nvs_storage->get_or_set_var(kNvsNamespace, "led_conn", s.led_connected_brightness,
                              s.led_connected_brightness, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "led_blink", s.led_activity_blink,
                              s.led_activity_blink, ec);
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

static bool save_settings(const device_config::Settings &s, std::string &error) {
  if (!nvs_storage) {
    error = "NVS is not initialized";
    return false;
  }
  // Stage every key on one handle and commit once, so a failure part-way
  // through never leaves a half-applied set of settings for the next boot.
  std::error_code ec;
  auto handle = nvs_storage->get_handle(kNvsNamespace, ec);
  auto stage = [&](const char *key, auto value) {
    if (!ec)
      handle.set(key, value, ec);
  };
  stage("inv_ly", s.invert_left_y);
  stage("inv_ry", s.invert_right_y);
  stage("swap_ab", s.swap_ab);
  stage("swap_xy", s.swap_xy);
  stage("deadzone", s.deadzone_percent);
  stage("led", s.led_brightness);
  stage("ble_name", s.ble_name);
  stage("led_conn", s.led_connected_brightness);
  stage("led_blink", s.led_activity_blink);
  if (!ec)
    handle.commit(ec);
  if (ec) {
    // uncommitted writes are discarded with the handle: nothing was persisted
    error = "could not save settings to NVS: " + ec.message();
    logger.error("{}", error);
    return false;
  }
  logger.info("Settings saved");
  return true;
}

// --- paired-controller names (NVS) --------------------------------------------------
//
// One namespace, one key per bond: the 12-hex-digit address (NVS keys are
// limited to 15 characters). Values are the controller's name (<= 31 chars).

static constexpr const char *kBondNamesNamespace = "bondnames";

static std::string bond_key(const std::array<uint8_t, 6> &address) {
  return fmt::format("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", address[0], address[1], address[2],
                     address[3], address[4], address[5]);
}

std::string services_bond_name(const std::array<uint8_t, 6> &address) {
  if (!nvs_storage)
    return {};
  std::string name;
  std::error_code ec;
  nvs_storage->get_var(kBondNamesNamespace, bond_key(address), name, ec);
  return ec ? std::string{} : name; // not found = unknown
}

void services_set_bond_name(const std::array<uint8_t, 6> &address, const std::string &name) {
  if (!nvs_storage || name.empty())
    return;
  if (services_bond_name(address) == name)
    return; // unchanged: spare the flash
  std::error_code ec;
  nvs_storage->set_var(kBondNamesNamespace, bond_key(address), name, ec);
  if (ec)
    logger.warn("Could not store controller name '{}': {}", name, ec.message());
  else
    logger.info("Stored controller name '{}'", name);
}

void services_forget_bond_name(const std::array<uint8_t, 6> &address) {
  if (!nvs_storage)
    return;
  std::error_code ec;
  nvs_storage->erase(kBondNamesNamespace, bond_key(address), ec); // missing key is fine
}

void services_clear_bond_names() {
  if (!nvs_storage)
    return;
  std::error_code ec;
  nvs_storage->erase(kBondNamesNamespace, ec);
}

// --- service instances -------------------------------------------------------------
//
// Each service is an espp DispatcherModuleConcept: it owns its protocol state
// machine and replies through the link's sender; registration is one call.

static std::unique_ptr<espp::Ota> ota;
static std::unique_ptr<espp::OtaService> ota_service;
static std::unique_ptr<espp::CoreDump> core_dump;
static std::unique_ptr<espp::CoreDumpService> coredump_service;
static std::unique_ptr<DeviceConfig> device_config_module;
static std::string crash_report;

// --- public API -----------------------------------------------------------------------

void services_init(espp::DispatcherWorker *link, const ServicesCallbacks &callbacks) {
  // NVS (settings + BLE bonds live here)
  nvs_storage = std::make_unique<espp::Nvs>();
  std::error_code ec;
  nvs_storage->init(ec);
  if (ec)
    logger.error("NVS init failed: {}", ec.message());
  const auto settings = load_settings();
  // no transport (plain HID build): replies have nowhere to go
  const auto send = link ? link->sender() : [](std::span<const uint8_t>) {};

  // --- OTA (module 0): espp::OtaService drives the engine; rollback is
  // host-driven (the console confirms a PENDING_VERIFY image) ---
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
  if (link) {
    ota_service = std::make_unique<espp::OtaService>(
        *ota, espp::OtaService::Config{.send = send, .log_level = espp::Logger::Verbosity::INFO});
    link->register_module(*ota_service);
  }

  // --- crash dumps (module 4) ---
  core_dump = std::make_unique<espp::CoreDump>();
  crash_report = core_dump->format_report();
  if (core_dump->has_core_dump())
    logger.warn("A crash core dump is stored:\n{}", crash_report);
  if (link) {
    coredump_service = std::make_unique<espp::CoreDumpService>(
        *core_dump,
        espp::CoreDumpService::Config{.send = send, .log_level = espp::Logger::Verbosity::INFO});
    link->register_module(*coredump_service);
  }

  // --- device configuration (module 0x10) ---
  device_config_module = std::make_unique<DeviceConfig>(DeviceConfig::Config{
      .send = send,
      .initial = settings,
      .on_settings_changed =
          [on_changed = callbacks.on_settings_changed](const device_config::Settings &s,
                                                       std::string &error) {
            if (!save_settings(s, error))
              return false; // not applied either: the module keeps the old values
            if (on_changed)
              on_changed(s);
            return true;
          },
      .info = callbacks.info,
      .on_action = callbacks.on_action,
      .bonds = callbacks.bonds,
      .forget_bond = callbacks.forget_bond,
      .log_level = espp::Logger::Verbosity::INFO});
  if (!link)
    return; // the module still holds + serves the settings for the app
  link->register_module(*device_config_module);

  // discovery: device name + firmware version, answered over the vendor stream
  link->serve_discovery("ESP USB BLE HID", running.version);
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  // bytes dropped on the transport: an in-flight image is unusable; the OTA
  // service aborts its session and tells the host (nothing else is stateful)
  usb_set_rx_overflow_callback([] { ota_service->on_rx_overflow(); });
#endif
}

device_config::Settings services_settings() {
  return device_config_module ? device_config_module->settings() : device_config::Settings{};
}

std::string services_crash_report() { return crash_report; }
