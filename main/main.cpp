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

// Apply the user settings to the controller inputs and push them into the USB
// controller's input report (streamed to the host by the USB sender task).
static void push_inputs(GamepadInputs inputs) {
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
  // otherwise this is a gamepad input report

  // set the data in the ble gamepad and convert it to GamepadInputs
  ble_gamepad->set_report_data(ble_gamepad->get_input_report_id(), pData, length);
  push_inputs(ble_gamepad->get_gamepad_inputs());

  if (usb_is_mounted()) {
    // toggle the LED each report, so mod 2
    static bool led_on = false;
    static const auto on_color = espp::Rgb(0.0f, 0.0f, 1.0f); // use blue for BLE
    static const auto off_color = espp::Rgb(0.0f, 0.0f, 0.0f);
    set_led(led_on ? on_color : off_color);
    led_on = !led_on;
  }
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
  info.battery_percent = static_cast<uint8_t>(battery_level_percent.load());
  info.bond_count = ble_bond_count();
  info.controller = get_serial_number();
  return info;
}

static bool device_action(device_config::Action action, std::string &error) {
  switch (action) {
  case device_config::Action::StartPairing:
    start_ble_pairing_thread(notifyCB);
    return true;
  case device_config::Action::ClearBonds:
    ble_clear_bonds();
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
    // the only per-controller detail we have is the connected one's serial
    if (b.connected)
      info.label = get_serial_number();
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
  if (ble_bond_count() == 0) {
    // nothing left to reconnect to: enter pairing mode
    start_ble_reconnection_thread(notifyCB);
  }
  return true;
}

static void apply_settings(const device_config::Settings &s) {
  set_led_brightness_percent(s.led_brightness);
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
  services_init(usb_dispatcher(), {.info = device_info,
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

  // MARK: USB initialization
  logger.info("USB initialization");
  if (!start_usb(usb_controller)) {
    logger.error("USB initialization failed");
  }

  // MARK: BLE initialization
  logger.info("BLE initialization (name '{}')", settings.ble_name);
  init_ble(settings.ble_name);

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
