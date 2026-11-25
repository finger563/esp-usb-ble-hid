#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "logger.hpp"
#include "task.hpp"
#include "timer.hpp"

#include "gamepad_inputs.hpp"
#include "gamepad_map.hpp"
#include "gamepad_state.hpp"
#include "gamepad_state_hid_rp.hpp"
#include "hid-rp-report-map.hpp"
#include "hid-rp-xbox.hpp"
#include "switch_pro.hpp"
#include "xinput.hpp"

#include "ble.hpp"
#include "bsp.hpp"
#include "rumble.hpp"
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
static std::shared_ptr<espp::SwitchPro> usb_controller;
static std::atomic<int> battery_level_percent{100};
// whether the standard Battery Service characteristic (exact percent) has
// delivered a level for the current controller: until then the controller's
// own battery report (quarters only) is used
static std::atomic<bool> have_bas_battery{false};
// whether the connected controller is the Xbox Wireless controller (Series
// X|S, model 1914, BLE 045E:0B13), whose own battery report (id 4) layout is
// known here; other controllers' reports go to the generic decoder (rumble is
// driven from each controller's report map, see rumble.cpp)
static std::atomic<bool> controller_is_xbox{false};
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

/********* Controller decoding + button map ***************/

// The connected controller's report decoder (built from the report map it
// reported when it connected) and the button map applied to it. Both are
// replaced from the BLE scan-timer task (connect) or the console (map edits)
// while reports are decoded on the NimBLE host task: one mutex.
static std::mutex decode_mutex;
static std::optional<espp::hid_rp::GamepadDecoder> decoder;
static espp::GamepadMap active_map;

// The decoder for a controller: its own report map, or (unreadable / not a
// gamepad) the Xbox Wireless controller's, which is what every controller was
// decoded as before.
static espp::hid_rp::GamepadDecoder make_decoder(const BleControllerInfo &info,
                                                 espp::Logger &logger) {
  std::optional<espp::hid_rp::ReportMap> map;
  if (!info.report_map.empty()) {
    map = espp::hid_rp::ReportMap::parse(info.report_map);
    if (map && !espp::hid_rp::GamepadDecoder::looks_like_gamepad(*map)) {
      logger.warn("'{}' report map does not describe a gamepad; decoding it as an Xbox controller",
                  info.name);
      map.reset();
    } else if (!map) {
      logger.warn("'{}' report map could not be parsed; decoding it as an Xbox controller",
                  info.name);
    }
  }
  if (!map) {
    const auto xbox = espp::xbox_descriptor();
    map = espp::hid_rp::ReportMap::parse(std::vector<uint8_t>(xbox.begin(), xbox.end()));
  }
  espp::hid_rp::GamepadDecoder d(std::move(*map));
  d.apply_quirks(info.vid, info.pid);
  // a bridge: every control reports only itself, the button map does the rest
  d.set_combine_controls(false);
  logger.info("'{}' ({:04x}:{:04x}): {} layout", info.name, info.vid, info.pid,
              espp::hid_rp::GamepadDecoder::layout_name(d.quirks().layout));
  return d;
}

// The controller's rumble encoder, from ITS OWN report map only: a controller
// whose map could not be read (and is decoded as an Xbox controller) is not
// assumed to rumble like one, so nothing is ever written to an output report
// the controller did not declare as rumble.
static std::optional<espp::hid_rp::RumbleEncoder> make_rumble_encoder(const BleControllerInfo &info,
                                                                      espp::Logger &logger) {
  if (info.report_map.empty())
    return std::nullopt;
  const auto map = espp::hid_rp::ReportMap::parse(info.report_map);
  auto encoder = map ? espp::hid_rp::RumbleEncoder::from_report_map(*map) : std::nullopt;
  if (encoder)
    logger.info("'{}': rumble report {} ({} actuators, effects up to {} ms)", info.name,
                encoder->report_id(), encoder->actuator_count(), encoder->max_duration_ms());
  else
    logger.info("'{}': its report map declares no rumble report", info.name);
  return encoder;
}

// The mapped state as the output stage's GamepadInputs. Positions keep the
// labels they always had: the controller's A (south) is reported as A, so a
// Switch shows its A where an Xbox pad has it (swap south / east in the map for
// the Nintendo layout). Sticks stay HID-style (Y down) here.
static GamepadInputs to_inputs(const espp::GamepadState &s) {
  using B = espp::GamepadState::Button;
  using A = espp::GamepadState::Axis;
  GamepadInputs in{};
  auto &b = in.buttons;
  b.a = s.pressed(B::South);
  b.b = s.pressed(B::East);
  b.x = s.pressed(B::West);
  b.y = s.pressed(B::North);
  b.l1 = s.pressed(B::L1);
  b.r1 = s.pressed(B::R1);
  b.l2 = s.pressed(B::L2);
  b.r2 = s.pressed(B::R2);
  b.l3 = s.pressed(B::L3);
  b.r3 = s.pressed(B::R3);
  b.up = s.pressed(B::Up);
  b.down = s.pressed(B::Down);
  b.left = s.pressed(B::Left);
  b.right = s.pressed(B::Right);
  b.home = s.pressed(B::Home);
  b.capture = s.pressed(B::Capture);
  b.start = s.pressed(B::Start);
  b.select = s.pressed(B::Select);
  constexpr float full = espp::GamepadState::AXIS_MAX;
  in.left_joystick = {s.axis(A::LeftX) / full, s.axis(A::LeftY) / full};
  in.right_joystick = {s.axis(A::RightX) / full, s.axis(A::RightY) / full};
  in.l2.value = s.axis(A::LeftTrigger) / full;
  in.r2.value = s.axis(A::RightTrigger) / full;
  return in;
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
  // the wireless controller. (Switch Pro mode only: the window is never opened
  // in Xbox 360 mode.)
  if (esp_timer_get_time() < registration_until_us) {
    inputs.buttons.l1 = 1;
    inputs.buttons.r1 = 1;
  }
  // The inputs arrive mapped (button map: swaps, dead zones, inversions...)
  // with HID-style sticks (Y down); both the Switch and X-Input want Y up.
  inputs.left_joystick.y = -inputs.left_joystick.y;
  inputs.right_joystick.y = -inputs.right_joystick.y;
  if (usb_personality() == device_config::UsbController::Xbox360) {
    // X-Input: the same physical layout as the wireless Xbox controller the
    // inputs come from, so buttons map one to one; the sticks are the settings-
    // adjusted values (with the default Y inversion, up is positive, which is
    // what X-Input expects) scaled to the 16-bit axes, the triggers to 8 bits.
    using espp::xinput::Button;
    espp::xinput::GamepadState st;
    const auto &b = inputs.buttons;
    st.set(Button::A, b.a);
    st.set(Button::B, b.b);
    st.set(Button::X, b.x);
    st.set(Button::Y, b.y);
    st.set(Button::LeftBumper, b.l1);
    st.set(Button::RightBumper, b.r1);
    st.set(Button::LeftStick, b.l3);
    st.set(Button::RightStick, b.r3);
    st.set(Button::DpadUp, b.up);
    st.set(Button::DpadDown, b.down);
    st.set(Button::DpadLeft, b.left);
    st.set(Button::DpadRight, b.right);
    st.set(Button::Start, b.start);
    st.set(Button::Back, b.select);
    st.set(Button::Guide, b.home);
    auto axis = [](float v) { return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f); };
    // A trigger follows its axis, or is fully pressed by its digital L2 / R2
    // button: a pad with on/off triggers reports only the button, and a button
    // map can target either one.
    auto trigger = [](float v, bool pressed) {
      return pressed ? uint8_t{255} : static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    };
    st.lx = axis(inputs.left_joystick.x);
    st.ly = axis(inputs.left_joystick.y);
    st.rx = axis(inputs.right_joystick.x);
    st.ry = axis(inputs.right_joystick.y);
    st.left_trigger = trigger(inputs.l2.value, b.l2);
    st.right_trigger = trigger(inputs.r2.value, b.r2);
    usb_push_xinput(st);
    return;
  }
  usb_controller->update_input_report([&](espp::SwitchPro::InputReport &r) {
    r.reset();
    r.set_buttons(inputs.buttons);
    r.set_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
    r.set_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
    // ZL / ZR are digital on a Pro Controller: pressed by the digital L2 / R2
    // button or by the trigger axis past half way. (set_brake / set_accelerator
    // assign ZL / ZR, so the button state set_buttons() wrote must be OR-ed in
    // here, or a button-only trigger would never reach the host.)
    r.set_brake(inputs.buttons.l2 || inputs.l2.value > 0.5f);
    r.set_accelerator(inputs.buttons.r2 || inputs.r2.value > 0.5f);
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
  if (usb_personality() != device_config::UsbController::SwitchPro)
    return; // an X-Input host has no registration screen
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
    if (length >= 1) {
      battery_level_percent = pData[0];
      have_bas_battery = true;
    }
    return;
  }
  // otherwise this is a HID report: which one is told by the characteristic's
  // Report Reference (a controller may notify several input reports; the
  // decoder keeps the latest of each and decodes them together)
  const uint8_t report_id = ble_report_id_for(pRemoteCharacteristic).value_or(1);
  // The Xbox Wireless controller's own battery report (id 4): the status
  // byte's bits 0-1 carry the capacity in quarters (the mapping Linux's
  // xpadneo driver reads). It stands in until the exact Battery Service level
  // arrives -- some controllers never expose that characteristic. Only the
  // Xbox pad's report id 4 is a battery report; on other controllers it is
  // ordinary input, so it goes to the decoder instead.
  if (controller_is_xbox.load() && report_id == espp::XboxBatteryInputReport<>::ID && length >= 1) {
    if (!have_bas_battery.load()) {
      static constexpr uint8_t kQuarterPercent[] = {0, 33, 66, 100};
      battery_level_percent = kQuarterPercent[pData[0] & 0x03];
    }
    return;
  }
  espp::GamepadState raw, mapped;
  {
    std::lock_guard<std::mutex> lk(decode_mutex);
    if (!decoder)
      return; // no controller info yet (cannot happen: it is read before subscribing)
    espp::hid_rp::GamepadReport report;
    if (!decoder->update(report_id, std::span<const uint8_t>(pData, length), report))
      return; // a report that carries no gamepad control (e.g. output-only)
    raw = espp::to_gamepad_state(report);
    active_map.apply(raw, mapped);
  }
  services_gamepad_input(raw, mapped);
  push_inputs(to_inputs(mapped));

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
  info.usb_controller = static_cast<uint8_t>(usb_personality());
  // a v4 image on a v3 partition table: the console offers the full update
  info.needs_full_flash = !services_gamepad_storage_available();
  // whether the connected controller declares a rumble report we can write
  info.rumble_supported = info.ble_connected && rumble_supported();
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

// The Device tab's Test rumble: the test pattern through the controller's
// saved rumble options.
static bool device_test_rumble(std::string &error) {
  switch (rumble_test()) {
  case RumbleTestResult::Ok:
    return true;
  case RumbleTestResult::NoController:
    error = "no controller is connected";
    return false;
  case RumbleTestResult::CannotRumble:
    error = "this controller does not declare a rumble report";
    return false;
  case RumbleTestResult::Off:
    error = "rumble is off for this controller (Button mapping tab)";
    return false;
  }
  error = "the rumble test could not be played";
  return false;
}

// The Button mapping tab's Test: the test pattern through the (unsaved)
// options the console shows; the saved ones are untouched. Returns at once.
static bool gamepad_test_rumble(const espp::RumbleOptions &options, std::string &error) {
  switch (rumble_test(options)) {
  case RumbleTestResult::Ok:
    return true;
  case RumbleTestResult::NoController:
    error = "no controller connected";
    return false;
  case RumbleTestResult::CannotRumble:
    error = "the connected controller cannot rumble";
    return false;
  case RumbleTestResult::Off:
    error = "rumble is off in these settings";
    return false;
  }
  error = "the rumble test could not be played";
  return false;
}

static bool device_action(device_config::Action action, std::string &error) {
  switch (action) {
  case device_config::Action::StartPairing:
    start_ble_pairing_thread(notifyCB);
    return true;
  case device_config::Action::ClearBonds:
    if (!ble_clear_bonds(error))
      return false; // keep the names: their bonds may still exist
    services_clear_bond_names();
    // no bonds left, so this enters pairing mode
    start_ble_reconnection_thread(notifyCB);
    return true;
  case device_config::Action::TestRumble:
    return device_test_rumble(error);
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
  if (!ble_forget_bond(address, address_type, error))
    return false;
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
  set_led_reconnecting_brightness_percent(s.led_reconnecting_brightness);
  set_led_pairing_brightness_percent(s.led_pairing_brightness);
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
  // The console's map / rumble changes arrive under the gamepad store's change
  // mutex and take decode_mutex, then (rumble) the sender's mutex: the same
  // order as a controller connecting (below), and the sender never takes
  // decode_mutex.
  services_init(vendor_link, {.info = device_info,
                              .on_action = device_action,
                              .bonds = device_bonds,
                              .forget_bond = device_forget_bond,
                              .on_settings_changed = apply_settings,
                              .on_gamepad_map_changed =
                                  [](const espp::GamepadMap &map) {
                                    std::lock_guard<std::mutex> lk(decode_mutex);
                                    active_map = map;
                                  },
                              .on_gamepad_rumble_changed =
                                  [](const espp::RumbleOptions &options) {
                                    std::lock_guard<std::mutex> lk(decode_mutex);
                                    rumble_set_options(options);
                                  },
                              .on_gamepad_test_rumble = gamepad_test_rumble});
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
  usb_controller = std::make_shared<espp::SwitchPro>(espp::SwitchPro::Config{
      // the Switch's HD rumble (every 0x01 / 0x10 report), played on the BLE
      // controller; runs on the TinyUSB task, rumble_set_target() does not block
      .on_rumble =
          [](const espp::hid_rp::SwitchRumble &r) { rumble_set_target(espp::to_rumble_state(r)); },
      .log_level = espp::Logger::Verbosity::WARN});

  // MARK: rumble sender (host rumble -> the BLE controller's motors)
  rumble_start();

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
  // a controller connecting: decode its reports with its own report map, map
  // them with its profile (or the default map), and play the host's rumble on
  // it with that profile's rumble options
  ble_set_controller_info_callback([&logger](const BleControllerInfo &info) {
    auto d = make_decoder(info, logger);
    auto enc = make_rumble_encoder(info, logger);
    // the Xbox Wireless controller (Series X|S, model 1914): its own battery
    // report layout is known (see notifyCB)
    controller_is_xbox = (info.vid == 0x045E && info.pid == 0x0B13);
    // the lookups happen under decode_mutex, which the console's change
    // callbacks take too: a change either lands before them or notifies after,
    // so the controller never keeps a stale map or stale rumble options. The
    // options are in place before the encoder, so the host's rumble never
    // plays on this controller with the previous one's options.
    std::lock_guard<std::mutex> lk(decode_mutex);
    const auto map = services_gamepad_connected(info.vid, info.pid, info.name);
    rumble_set_options(services_gamepad_rumble());
    rumble_set_encoder(std::move(enc));
    decoder = std::move(d);
    active_map = map;
  });
  ble_set_disconnect_callback([]() {
    battery_level_percent = 0;
    have_bas_battery = false;
    controller_is_xbox = false;
    rumble_set_encoder(std::nullopt); // nothing to drive; the next controller starts off
    {
      std::lock_guard<std::mutex> lk(decode_mutex);
      if (decoder)
        decoder->reset(); // the next connection starts from no reports
    }
    services_gamepad_disconnected();
    push_inputs(GamepadInputs{});
  });
  // rumble: the X-Input host's rumble, and an all-off target when the host
  // unmounts or suspends (usb.cpp); the sender plays it on the controller
  usb_set_rumble_callback([](const espp::RumbleState &rumble) { rumble_set_target(rumble); });

  // MARK: USB initialization -- as the controller the settings ask for
  // (descriptors are fixed at enumeration, so a changed setting applies at
  // the next boot; the console says so)
  const auto personality = settings.usb_controller_kind();
  logger.info("USB initialization ({})", personality == device_config::UsbController::Xbox360
                                             ? "Xbox 360 / X-Input"
                                             : "Switch Pro");
  if (!start_usb(personality, usb_controller)) {
    logger.error("USB initialization failed");
  }
#if HAS_DISPLAY
  // the screen's USB side shows what the dongle is to the host
  gui->set_usb_host(personality == device_config::UsbController::Xbox360 ? Gui::UsbHost::Xbox360
                                                                         : Gui::UsbHost::SwitchPro);
#endif // HAS_DISPLAY
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
