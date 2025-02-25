#include <chrono>
#include <thread>

#include "logger.hpp"
#include "task.hpp"

#include "switch_pro.hpp"
#include "xbox.hpp"

#include "ble.hpp"
#include "bsp.hpp"
#include "usb.hpp"

using namespace std::chrono_literals;

/************* App Configuration ****************/

static std::shared_ptr<Gui> gui;
static std::vector<uint8_t> hid_report_descriptor;
std::shared_ptr<GamepadDevice> ble_gamepad;
std::shared_ptr<GamepadDevice> usb_gamepad;

/********* BLE callbacks ***************/

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length,
              bool isNotify) {
  // set the data in the ble gamepad
  ble_gamepad->set_report_data(ble_gamepad->get_input_report_id(), pData, length);

  // convert it to GamepadInputs
  auto inputs = ble_gamepad->get_gamepad_inputs();

  // invert the y-axis for the joysticks
  inputs.left_joystick.y = -inputs.left_joystick.y;
  inputs.right_joystick.y = -inputs.right_joystick.y;

  // now set the data in the usb gamepad
  usb_gamepad->set_gamepad_inputs(inputs);

  // then get the output report from the usb gamepad
  uint8_t usb_report_id = usb_gamepad->get_input_report_id();
  auto report = usb_gamepad->get_report_data(usb_report_id);

  // send the report via tiny usb
  if (tud_mounted()) {
    // and send it over USB
    send_hid_report(usb_report_id, report);

    // toggle the LED each send, so mod 2
    static auto &bsp = Bsp::get();
    static bool led_on = false;
    static auto on_color = espp::Rgb(0.0f, 0.0f, 1.0f); // use blue for BLE
    static auto off_color = espp::Rgb(0.0f, 0.0f, 0.0f);
    bsp.led(led_on ? on_color : off_color);
    led_on = !led_on;
  }
}

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "ESP USB BLE HID", .level = espp::Logger::Verbosity::DEBUG});

  logger.info("Bootup");

  // MARK: BSP initialization
  auto &bsp = Bsp::get();

  // MARK: LED initialization
  bsp.initialize_led();
  bsp.led(espp::Rgb(0.0f, 0.0f, 0.0f));

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

  // MARK: BLE pairing timer (for use with button)
  espp::HighResolutionTimer ble_pairing_timer{
      {.name = "Pairing Timer", .callback = [&]() { start_ble_pairing_thread(notifyCB); }}};

  // MARK: Pairing button initialization
  // initialize the button, which we'll use to cycle the rotation of the display
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
  usb_gamepad = std::make_shared<SwitchPro>();
  ble_gamepad = std::make_shared<Xbox>();

  // MARK: USB initialization
  logger.info("USB initialization");
#if DEBUG_USB
  set_gui(gui);
#endif // DEBUG_USB
  start_usb_gamepad(usb_gamepad);

  // MARK: BLE initialization
  logger.info("BLE initialization");
  init_ble();

  logger.info("Scanning for peripherals");
  start_ble_reconnection_thread(notifyCB);

  // Loop here until we find a device we want to connect to
  while (true) {
    // sleep for a bit
    std::this_thread::sleep_for(1s);

    // update the display if we have one
#if HAS_DISPLAY
    // show the usb icon if the USB is mounted
    gui->set_usb_connected(tud_mounted());
    // show the BLE icon if the BLE subsystem is subscribed (receiving data)
    gui->set_ble_connected(is_ble_subscribed());
#endif // HAS_DISPLAY

    // if we're subscribed, then don't do anything else
    if (is_ble_subscribed()) {
      continue;
    }

    GamepadInputs inputs{};

    // otherwise, just twirl the joysticks
    static constexpr int num_segments = 16;
    static int index = 0;
    float angle = 2.0f * M_PI * (index % num_segments) / (num_segments);
    // joystick inputs are in the range [-1, 1] float
    inputs.left_joystick.x = sin(angle);
    inputs.left_joystick.y = cos(angle);
    inputs.right_joystick.x = cos(angle);
    inputs.right_joystick.y = sin(angle);

    // // NOTE: this is commented out since it's annoying when it works, but left
    // // in for debugging when it doesn't work.
    // static constexpr int num_buttons = 15;
    // inputs.set_button(index % num_buttons, true);

    index++;

    // set the inputs in the usb gamepad
    usb_gamepad->set_gamepad_inputs(inputs);

    // get the output report from the usb gamepad
    uint8_t usb_report_id = usb_gamepad->get_input_report_id();
    auto report = usb_gamepad->get_report_data(usb_report_id);

    if (tud_mounted()) {
      bool success = send_hid_report(usb_report_id, report);
    } else {
      bsp.led(espp::Rgb(1.0f, 0.0f, 0.0f));
    }
  }
}
