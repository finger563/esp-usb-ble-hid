#include "services.hpp"

#include <chrono>
#include <memory>
#include <mutex>

#include "esp_partition.h"
#include "esp_system.h"

#include "coredump.hpp"
#include "coredump_service.hpp"
#include "file_system.hpp"
#include "format.hpp"
#include "gamepad_map_service.hpp"
#include "gamepad_profiles.hpp"
#include "logger.hpp"
#include "monitor_service.hpp"
#include "nvs.hpp"
#include "ota.hpp"
#include "ota_service.hpp"
#include "system_service.hpp"

#include "usb.hpp"

using namespace std::chrono_literals;

static espp::Logger logger({.tag = "Services", .level = espp::Logger::Verbosity::INFO});

// --- settings persistence (NVS) --------------------------------------------------

static constexpr const char *kNvsNamespace = "dongle";
static std::unique_ptr<espp::Nvs> nvs_storage;

static device_config::Settings default_settings() {
  device_config::Settings s;
#if CONFIG_DONGLE_DEFAULT_XBOX360
  s.usb_controller = static_cast<uint8_t>(device_config::UsbController::Xbox360);
#endif
  return s;
}

static device_config::Settings load_settings() {
  device_config::Settings s = default_settings();
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
  nvs_storage->get_or_set_var(kNvsNamespace, "led_recon", s.led_reconnecting_brightness,
                              s.led_reconnecting_brightness, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "led_pair", s.led_pairing_brightness,
                              s.led_pairing_brightness, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "led_blink", s.led_activity_blink,
                              s.led_activity_blink, ec);
  nvs_storage->get_or_set_var(kNvsNamespace, "usb_ctl", s.usb_controller, s.usb_controller, ec);
  if (ec)
    logger.warn("Could not read all settings from NVS: {}", ec.message());
  // never trust stored values blindly (an older firmware may have stored
  // something this one rejects)
  if (auto why = s.validate(); !why.empty()) {
    logger.warn("Stored settings invalid ({}); using defaults", why);
    s = default_settings();
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
  stage("led_recon", s.led_reconnecting_brightness);
  stage("led_pair", s.led_pairing_brightness);
  stage("led_blink", s.led_activity_blink);
  stage("usb_ctl", s.usb_controller);
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

// --- gamepad maps (LittleFS) --------------------------------------------------------
//
// The maps live in a file (espp::GamepadProfiles, saved with PersistentData) on
// the `littlefs` partition. A dongle updated over USB from firmware older than
// that partition still has the old partition table (an OTA cannot change it):
// it must not mount (espp::FileSystem aborts when the partition is missing), so
// it keeps the maps in RAM and the console offers the full update.

static constexpr const char *kFsPartitionLabel = "littlefs";
static bool gamepad_storage = false;
static std::unique_ptr<espp::GamepadProfiles> gamepad_profiles;
static std::unique_ptr<espp::GamepadMapService> gamepad_map_service;

static bool fs_partition_present() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
                                  kFsPartitionLabel) != nullptr;
}

// The input settings older firmware kept in NVS, as a map. The Switch and
// X-Input both want stick Y up-positive, which the output stage now always
// converts to; the old "invert Y" settings (on by default) did that, so only a
// stick whose inversion was turned OFF needs a rule (an extra inversion).
static espp::GamepadMap legacy_settings_map(const device_config::Settings &s) {
  using B = espp::GamepadState::Button;
  using A = espp::GamepadState::Axis;
  espp::GamepadMap m;
  m.left_deadzone_percent = s.deadzone_percent;
  m.right_deadzone_percent = s.deadzone_percent;
  if (s.swap_ab)
    m.swap(B::South, B::East);
  if (s.swap_xy)
    m.swap(B::West, B::North);
  if (!s.invert_left_y)
    m.invert(A::LeftY);
  if (!s.invert_right_y)
    m.invert(A::RightY);
  return m;
}

static void init_gamepad_maps(const device_config::Settings &settings) {
  const auto legacy = legacy_settings_map(settings);
  gamepad_storage = fs_partition_present();
  std::filesystem::path file;
  if (gamepad_storage) {
    file = espp::FileSystem::get().get_root_path() / "gamepad_profiles.bin";
  } else {
    logger.warn("No '{}' partition (flashed before v4?): button maps are not saved across "
                "reboots until the dongle is fully re-flashed",
                kFsPartitionLabel);
    file = "/no-littlefs-partition/gamepad_profiles.bin"; // every save fails, loudly
  }
  const bool migrate = gamepad_storage && !std::filesystem::exists(file) && !legacy.is_identity();
  gamepad_profiles = std::make_unique<espp::GamepadProfiles>(espp::GamepadProfiles::Config{
      .file_path = file, .default_map = legacy, .log_level = espp::Logger::Verbosity::INFO});
  if (migrate) {
    // first boot of a dongle that had customized its inputs: keep them, as the
    // default map, now that the map replaces those settings
    std::error_code ec;
    if (gamepad_profiles->set_default_map(legacy, ec))
      logger.info("Migrated the input settings to the default button map ({} rules, dead zone {}%)",
                  legacy.rules.size(), legacy.left_deadzone_percent);
    else
      logger.warn("Could not save the migrated button map: {}", ec.message());
  }
}

// --- service instances -------------------------------------------------------------
//
// Each service is an espp DispatcherModuleConcept: it owns its protocol state
// machine and replies through the link's sender; registration is one call.

static std::unique_ptr<espp::Ota> ota;
static std::unique_ptr<espp::OtaService> ota_service;
static std::unique_ptr<espp::CoreDump> core_dump;
static std::unique_ptr<espp::CoreDumpService> coredump_service;
static std::unique_ptr<espp::SystemService> system_service;
static std::unique_ptr<espp::MonitorService> monitor_service;
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

  // --- gamepad button maps (module 11): the console edits them, the input
  // path applies them ---
  init_gamepad_maps(settings);
  gamepad_map_service = std::make_unique<espp::GamepadMapService>(
      espp::GamepadMapService::Config{.send = send,
                                      .profiles = gamepad_profiles.get(),
                                      .on_active_map_changed = callbacks.on_gamepad_map_changed,
                                      .log_level = espp::Logger::Verbosity::INFO});
  if (link)
    link->register_module(*gamepad_map_service);

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

  // --- system (module 7): identity / status, reboot, reboot into the ROM
  // bootloader (download mode: the dongle re-enumerates as the S3 ROM's USB
  // port, so esptool / `idf.py flash` work without touching the BOOT button,
  // handy for a dongle that lives in a Switch). usb_persist stays off: the
  // composite HID + vendor device is not ROM-CDC/DFU-compatible. ---
  if (link) {
    system_service = std::make_unique<espp::SystemService>(espp::SystemService::Config{
        .send = send,
        .on_reboot_request =
            [](espp::SystemService::RebootKind kind) {
              logger.warn("Console requested a {}; allowing it",
                          kind == espp::SystemService::RebootKind::Bootloader
                              ? "reboot into the bootloader"
                              : "reboot");
              return true;
            },
        .log_level = espp::Logger::Verbosity::INFO});
    link->register_module(*system_service);
    // --- monitor (module 8): heap regions + the task table, on request or
    // streamed (needs the FreeRTOS run-time stats, see sdkconfig.defaults) ---
    monitor_service = std::make_unique<espp::MonitorService>(
        espp::MonitorService::Config{.send = send, .log_level = espp::Logger::Verbosity::INFO});
    link->register_module(*monitor_service);
  }

  // --- device configuration (module 0x10) ---
  device_config_module = std::make_unique<DeviceConfig>(DeviceConfig::Config{
      .send = send,
      .initial = settings,
      .defaults = default_settings(),
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

espp::GamepadMap services_gamepad_connected(uint16_t vid, uint16_t pid, const std::string &name) {
  return gamepad_map_service ? gamepad_map_service->set_controller(vid, pid, name)
                             : espp::GamepadMap{};
}

void services_gamepad_disconnected() {
  if (gamepad_map_service)
    gamepad_map_service->clear_controller();
}

void services_gamepad_input(const espp::GamepadState &raw, const espp::GamepadState &mapped) {
  if (gamepad_map_service)
    gamepad_map_service->publish_input(raw, mapped);
}

bool services_gamepad_storage_available() { return gamepad_storage; }
