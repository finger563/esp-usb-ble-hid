#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "logger.hpp"
#include "task.hpp"
#include "timer.hpp"

#include "switch_pro.hpp"
#include "xbox.hpp"

#include "ble.hpp"
#include "bsp.hpp"
#include "services.hpp"
#include "status_led.hpp"
#include "usb.hpp"

// set to 1 to enable twirling the joysticks automatically (for testing) when
// there is no BLE device connected.
#define DEBUG_NO_BLE_TWIRL_JOYSTICKS 0
// set to 1 to enable pushing the buttons automatically (for testing) when there
// is no BLE device connected. Not recommended unless you want to annoy
// yourself.
#define DEBUG_NO_BLE_TEST_BUTTONS 0

using namespace std::chrono_literals;

/************* App Configuration ****************/

#if HAS_DISPLAY
static std::shared_ptr<Gui> gui;
#endif
static std::shared_ptr<GamepadDevice> ble_gamepad;
static std::shared_ptr<espp::SwitchPro> usb_controller;
static std::atomic<int> battery_level_percent{100};
static std::mutex serial_number_mutex;
static std::string serial_number = "";

static std::string get_serial_number() {
  std::lock_guard<std::mutex> lk(serial_number_mutex);
  return serial_number;
}
static void set_serial_number(const std::string &sn) {
  std::lock_guard<std::mutex> lk(serial_number_mutex);
  serial_number = sn;
}

/********* Input mapping ***************/

// Radial deadzone: inside it the stick reads (0,0); outside it the remaining
// travel is rescaled so full deflection still reaches 1.0.
static void apply_deadzone(GamepadInputs::Joystick &j, float deadzone) {
  if (deadzone <= 0.0f)
    return;
  const float mag = std::sqrt(j.x * j.x + j.y * j.y);
  if (mag < deadzone) {
    j.x = j.y = 0.0f;
    return;
  }
  const float scale = std::min(1.0f, (mag - deadzone) / (1.0f - deadzone)) / mag;
  j.x *= scale;
  j.y *= scale;
}

// The last inputs pushed (before settings), so the report can be re-pushed by
// the USB registration timer below; and the end of the registration window
// (0 = none) during which L+R are held. One mutex serializes every report
// push with the window transitions, so a notification arriving between the
// timer's snapshot and its re-push can never be overwritten by the snapshot.
static std::mutex inputs_mutex;
static GamepadInputs last_inputs{};
static int64_t registration_until_us{0}; // guarded by inputs_mutex
static constexpr int64_t kRegistrationWindowUs = 1000 * 1000;

// Apply the user settings to the controller inputs and push them into the USB
// controller's input report (streamed to the host by the USB sender task).
static void push_inputs_locked(GamepadInputs inputs) {
  // The Switch's Change Grip/Order screen registers a controller when L+R are
  // pressed: hold them for a moment after every USB enumeration (see
  // usb_registration_tick) so the dongle registers itself without a hand on
  // the wireless controller.
  if (esp_timer_get_time() < registration_until_us) {
    inputs.buttons.l1 = 1;
    inputs.buttons.r1 = 1;
  }
  const auto s = services_settings();
  if (s.invert_left_y)
    inputs.left_joystick.y = -inputs.left_joystick.y;
  if (s.invert_right_y)
    inputs.right_joystick.y = -inputs.right_joystick.y;
  const float deadzone = s.deadzone_percent / 100.0f;
  apply_deadzone(inputs.left_joystick, deadzone);
  apply_deadzone(inputs.right_joystick, deadzone);
  if (s.swap_ab) {
    const bool a = inputs.buttons.a, b = inputs.buttons.b;
    inputs.buttons.a = b;
    inputs.buttons.b = a;
  }
  if (s.swap_xy) {
    const bool x = inputs.buttons.x, y = inputs.buttons.y;
    inputs.buttons.x = y;
    inputs.buttons.y = x;
  }
  usb_controller->update_input_report([&](espp::SwitchPro::InputReport &r) {
    r.reset();
    r.set_buttons(inputs.buttons);
    r.set_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
    r.set_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
    r.set_brake(inputs.l2.value);
    r.set_accelerator(inputs.r2.value);
  });
  usb_controller->set_battery_level(static_cast<uint8_t>(battery_level_percent.load()));
}

static void push_inputs(GamepadInputs inputs) {
  std::lock_guard<std::mutex> lk(inputs_mutex);
  last_inputs = inputs;
  push_inputs_locked(inputs);
}

// Polled every 50 ms: when the host has just finished the handshake and
// started taking reports, hold L+R for kRegistrationWindowUs, then release.
static void usb_registration_tick() {
  static bool was_ready = false;
  const bool ready = usb_hid_ready();
  const int64_t now = esp_timer_get_time();
  // the window transition and the re-push happen under the same lock as every
  // other push, so the latest inputs are what gets (re)pushed
  std::lock_guard<std::mutex> lk(inputs_mutex);
  if (ready && !was_ready) {
    registration_until_us = now + kRegistrationWindowUs;
    push_inputs_locked(last_inputs); // with L+R
  } else if (!ready) {
    registration_until_us = 0;
  } else if (registration_until_us && now >= registration_until_us) {
    registration_until_us = 0;
    push_inputs_locked(last_inputs); // release L+R
  }
  was_ready = ready;
}

/********* BLE callbacks ***************/

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length,
              bool isNotify) {
  // if it's the battery level characteristic, then store the battery level and
  // return.
  if (pRemoteCharacteristic->getUUID().equals(
          NimBLEUUID(espp::BatteryService::BATTERY_LEVEL_CHAR_UUID))) {
    battery_level_percent = pData[0];
    return;
  }
  // otherwise this is a HID report: which one is told by the characteristic's
  // Report Reference (a controller may notify several input reports); an
  // unknown characteristic is assumed to be the main gamepad input report
  const uint8_t report_id =
      ble_report_id_for(pRemoteCharacteristic).value_or(ble_gamepad->get_input_report_id());
  ble_gamepad->set_report_data(report_id, pData, length);
  if (report_id != ble_gamepad->get_input_report_id())
    return; // not the gamepad input report: nothing to forward

  // convert it to GamepadInputs and push it to the USB controller
  push_inputs(ble_gamepad->get_gamepad_inputs());

  // Debugging aid (off by default): toggle the LED on every report that
  // reaches the USB side, so input activity is visible on the dongle. The
  // default is the steady "connected" level, owned by the BLE link supervisor.
  if (usb_is_mounted())
    led_blink_toggle(); // no-op unless the blink setting is on
}

/********* Dongle console (device-config module) callbacks ***************/

static device_config::Info device_info() {
  device_config::Info info;
  const auto *desc = esp_app_get_description();
  info.project = desc->project_name;
  info.firmware = desc->version;
  info.idf_version = desc->idf_ver;
  info.hardware = HARDWARE_NAME;
  info.uptime_s = static_cast<uint32_t>(esp_timer_get_time() / 1000000);
  info.usb_mounted = usb_is_mounted();
  info.ble_connected = is_ble_subscribed();
  info.ble_scanning = is_ble_scanning();
  info.pairing = is_ble_pairing();
  info.bond_count = ble_bond_count();
  // battery / serial describe the connected controller only: report them as
  // unknown (0 / "") rather than stale cached values when nothing is connected
  if (info.ble_connected) {
    info.battery_percent = static_cast<uint8_t>(battery_level_percent.load());
    info.controller = get_serial_number();
  }
  // link diagnostics: is the controller actually sending, and is the Switch
  // actually consuming what we stream?
  info.ble_notifications = ble_notification_count();
  info.ble_last_notify_age_ms = ble_ms_since_last_notification();
  info.usb_hid_reports = usb_hid_reports_sent();
  info.usb_hid_ready = usb_hid_ready();
  // where the controller link is (and why it is there), for the console
  using LinkState = device_config::Info::LinkState;
  switch (ble_link_state()) {
  case BleLinkState::Idle:
    info.link_state = static_cast<uint8_t>(LinkState::Idle);
    break;
  case BleLinkState::Scanning:
    info.link_state = static_cast<uint8_t>(LinkState::Scanning);
    break;
  case BleLinkState::Connecting:
    info.link_state = static_cast<uint8_t>(LinkState::Connecting);
    break;
  case BleLinkState::Encrypting:
    info.link_state = static_cast<uint8_t>(LinkState::Encrypting);
    break;
  case BleLinkState::Subscribing:
    info.link_state = static_cast<uint8_t>(LinkState::Subscribing);
    break;
  case BleLinkState::Subscribed:
    info.link_state = static_cast<uint8_t>(LinkState::Subscribed);
    break;
  }
  info.link_detail = ble_link_detail();
  return info;
}

static bool device_action(device_config::Action action, std::string &error) {
  switch (action) {
  case device_config::Action::StartPairing:
    start_ble_pairing_thread(notifyCB);
    return true;
  case device_config::Action::ClearBonds:
    if (!ble_clear_bonds()) {
      error = "could not clear the bond store";
      return false; // keep the names: their bonds may still exist
    }
    services_clear_bond_names();
    // no bonds left, so this enters pairing mode
    start_ble_reconnection_thread(notifyCB);
    return true;
  case device_config::Action::Reboot:
    // let the OK reply reach the host first
    std::thread([]() {
      std::this_thread::sleep_for(500ms);
      esp_restart();
    }).detach();
    return true;
  }
  error = "unknown action";
  return false;
}

static std::vector<device_config::BondInfo> device_bonds() {
  std::vector<device_config::BondInfo> bonds;
  for (const auto &b : ble_bonds()) {
    device_config::BondInfo info;
    info.address = b.address;
    info.address_type = b.address_type;
    info.connected = b.connected;
    // the name the controller reported when it last connected ("" = unknown)
    info.label = services_bond_name(b.address);
    bonds.push_back(std::move(info));
  }
  return bonds;
}

static bool device_forget_bond(const std::array<uint8_t, 6> &address, uint8_t address_type,
                               std::string &error) {
  if (!ble_forget_bond(address, address_type)) {
    error = "no such paired controller";
    return false;
  }
  services_forget_bond_name(address);
  if (ble_bond_count() == 0) {
    // nothing left to reconnect to: enter pairing mode
    start_ble_reconnection_thread(notifyCB);
  }
  return true;
}

static void apply_settings(const device_config::Settings &s) {
  set_led_brightness_percent(s.led_brightness);
  set_led_connected_brightness_percent(s.led_connected_brightness);
  set_led_activity_blink(s.led_activity_blink);
  // the other settings are read per input report (push_inputs); the BLE name
  // is read at boot (init_ble)
}

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "ESP USB BLE HID", .level = espp::Logger::Verbosity::DEBUG});

  logger.info("Bootup");

  // MARK: BSP initialization
  auto &bsp = Bsp::get();

  // MARK: LED initialization
  bsp.initialize_led();
  set_led(espp::Rgb(0.0f, 0.0f, 0.0f));

  // MARK: Display initialization
#if HAS_DISPLAY
  logger.info("Display initialization");
  // initialize the LCD
  if (!bsp.initialize_lcd()) {
    logger.error("Failed to initialize LCD!");
    return;
  }
  // set the pixel buffer to be a full screen buffer
  static constexpr size_t pixel_buffer_size = bsp.lcd_width() * bsp.lcd_height();
  // initialize the LVGL display for the T-Dongle-S3
  if (!bsp.initialize_display(pixel_buffer_size)) {
    logger.error("Failed to initialize display!");
    return;
  }

  // initialize the gui
  logger.info("Making GUI");
  gui = std::make_shared<Gui>(Gui::Config{.log_level = espp::Logger::Verbosity::INFO});
  gui->set_label_text("");
#else  // HAS_DISPLAY
  logger.info("No display");
#endif // HAS_DISPLAY

  // MARK: Services (NVS settings, OTA, crash dumps, device config on the USB
  // vendor stream). Registered before USB starts so the modules exist as soon
  // as the host can talk to us.
  logger.info("Services initialization");
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  auto *vendor_link = &usb_dispatcher();
#else
  espp::DispatcherWorker *vendor_link = nullptr; // plain HID build: no console stream
#endif
  services_init(vendor_link, {.info = device_info,
                              .on_action = device_action,
                              .bonds = device_bonds,
                              .forget_bond = device_forget_bond,
                              .on_settings_changed = apply_settings});
  const auto settings = services_settings();
  apply_settings(settings);
  if (const auto report = services_crash_report(); !report.empty())
    logger.warn("Previous boot crashed:\n{}", report);

  // MARK: BLE pairing timer (for use with button)
  espp::HighResolutionTimer ble_pairing_timer{
      {.name = "Pairing Timer", .callback = [&]() { start_ble_pairing_thread(notifyCB); }}};

  // MARK: Pairing button initialization
  logger.info("Initializing the button");
  auto on_button_pressed = [&](const auto &event) {
    if (event.active) {
      // start ble pairing timer
      ble_pairing_timer.oneshot(3'000'000); // 3 seconds
    } else {
      // cancel the ble pairing timer
      ble_pairing_timer.stop();
    }
  };
  bsp.initialize_button(on_button_pressed);

  // MARK: Gamepad initialization
  usb_controller = std::make_shared<espp::SwitchPro>(
      espp::SwitchPro::Config{.log_level = espp::Logger::Verbosity::WARN});
  ble_gamepad = std::make_shared<Xbox>();

  // MARK: BLE initialization -- before USB: as soon as the device is mounted
  // the console can ask for status / bonds / actions, which use NimBLE.
  logger.info("BLE initialization (name '{}')", settings.ble_name);
  init_ble(settings.ble_name);
  // remember each controller's name so the console can list it by name
  ble_set_bond_name_callback(
      [](const std::array<uint8_t, 6> &address, uint8_t, const std::string &name) {
        services_set_bond_name(address, name);
      });
  // when the controller goes away (powered off, out of range, ...), release
  // every button and center the sticks: the Switch would otherwise keep seeing
  // whatever was last reported, forever
  ble_set_disconnect_callback([]() {
    battery_level_percent = 0;
    push_inputs(GamepadInputs{});
  });

  // MARK: USB initialization
  logger.info("USB initialization");
  if (!start_usb(usb_controller)) {
    logger.error("USB initialization failed");
  }
  // hold L+R for a moment after every USB enumeration so the Switch's Change
  // Grip/Order screen registers the dongle (see usb_registration_tick)
  static espp::Timer usb_registration_timer({.name = "usb-reg",
                                             .period = 50ms,
                                             .callback =
                                                 [] {
                                                   usb_registration_tick();
                                                   return false;
                                                 },
                                             .log_level = espp::Logger::Verbosity::WARN});

  logger.info("Scanning for peripherals");
  start_ble_reconnection_thread(notifyCB);

  // Loop here until we find a device we want to connect to
  while (true) {
    // sleep for a bit
    std::this_thread::sleep_for(1s);

    // update the display if we have one
#if HAS_DISPLAY
    // show the usb icon if the USB is mounted
    gui->set_usb_connected(usb_is_mounted());
    // show the BLE icon if the BLE subsystem is subscribed (receiving data)
    gui->set_ble_connected(is_ble_subscribed());
#endif // HAS_DISPLAY

    // if we're subscribed, then don't do anything else
    if (is_ble_subscribed()) {
      // if we haven't gotten the serial number, then get that and save it
      if (get_serial_number().empty()) {
        const auto sn = get_connected_client_serial_number();
        set_serial_number(sn);
#if HAS_DISPLAY
        gui->set_label_text(sn);
#endif // HAS_DISPLAY
      }
      continue;
    }
    // make sure to reset the connected device serial number
    set_serial_number("");
#if HAS_DISPLAY
    gui->set_label_text("");
#endif // HAS_DISPLAY

#if DEBUG_NO_BLE_TWIRL_JOYSTICKS
    // otherwise, just twirl the joysticks
    static constexpr int num_segments = 16;
    static int index = 0;
    float angle = 2.0f * M_PI * (index % num_segments) / (num_segments);

    GamepadInputs inputs{};
    // joystick inputs are in the range [-1, 1] float
    inputs.left_joystick.x = sin(angle);
    inputs.left_joystick.y = cos(angle);
    inputs.right_joystick.x = cos(angle);
    inputs.right_joystick.y = sin(angle);

#if DEBUG_NO_BLE_TEST_BUTTONS
    // NOTE: this is not recommended since it's annoying when it works, but left
    // in for debugging when it doesn't work.
    static constexpr int num_buttons = 15;
    inputs.set_button(index % num_buttons, true);
#endif // DEBUG_NO_BLE_TEST_BUTTONS

    index++;

    push_inputs(inputs);
    if (!usb_is_mounted()) {
      set_led(espp::Rgb(1.0f, 0.0f, 0.0f));
    }
#endif // DEBUG_NO_BLE_TWIRL_JOYSTICKS
  }
}
