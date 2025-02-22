#include <chrono>
#include <thread>

#include "logger.hpp"
#include "task.hpp"

#if CONFIG_TARGET_HARDWARE_QTPY_ESP32_S3
#define HAS_DISPLAY 0
#include "qtpy.hpp"
using Bsp = espp::QtPy;
#elif CONFIG_TARGET_HARDWARE_T3_DONGLE
#define HAS_DISPLAY 1
#include "t-dongle-s3.hpp"
using Bsp = espp::TDongleS3;
#else
#error "No hardware target specified"
#endif

#if HAS_DISPLAY
#include "gui.hpp"
#endif

#include "switch_pro.hpp"
#include "xbox.hpp"

#include "ble.hpp"
#include "usb.hpp"

using namespace std::chrono_literals;

/************* App Configuration ****************/

std::shared_ptr<Gui> gui;
static std::vector<uint8_t> hid_report_descriptor;
std::shared_ptr<GamepadDevice> ble_gamepad;
std::shared_ptr<GamepadDevice> usb_gamepad;

/********* BLE callbacks ***************/

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length,
              bool isNotify) {
  std::string str = (isNotify == true) ? "Notification" : "Indication";
  str += " from ";
  str += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
  str += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
  str += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
  // str             += ", Value = " + std::string((char*)pData, length);
  fmt::print("{}\n", str);
  std::vector<uint8_t> data(pData, pData + length);

  // set the data in the ble gamepad
  ble_gamepad->set_report_data(ble_gamepad->get_input_report_id(), pData, length);

  // convert it to GamepadInputs
  auto inputs = ble_gamepad->get_gamepad_inputs();

  // now set the data in the usb gamepad
  usb_gamepad->set_gamepad_inputs(inputs);

  // then get the output report from the usb gamepad
  uint8_t usb_report_id = usb_gamepad->get_input_report_id();
  auto report = usb_gamepad->get_report_data(usb_report_id);

  // send the report via tiny usb
  if (tud_mounted()) {
    // and send it over USB
    tud_hid_report(usb_report_id, report.data(), report.size());

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
#else
  logger.info("No display");
#endif

  // MARK: Gamepad initialization
  usb_gamepad = std::make_shared<Xbox>();
  ble_gamepad = std::make_shared<Xbox>();

  // MARK: USB initialization
  logger.info("USB initialization");
  start_usb_gamepad(usb_gamepad);

  // MARK: BLE initialization
  logger.info("BLE initialization");
  init_ble();

  logger.info("Scanning for peripherals");
  NimBLEUUID hid_service_uuid(espp::HidService::SERVICE_UUID);
  NimBLEUUID report_map_uuid(espp::HidService::REPORT_MAP_UUID);
  NimBLEUUID hid_input_uuid(espp::HidService::REPORT_UUID);
  start_ble_scan_thread(hid_service_uuid, hid_input_uuid, notifyCB);

  // Loop here until we find a device we want to connect to
  while (true) {
    // sleep for a bit
    std::this_thread::sleep_for(1s);

    // if we're subscribed, then don't do anything else
    if (is_ble_subscribed()) {
      continue;
    }

    // otherwise, just twirl the joysticks
    static constexpr int num_segments = 15;
    static int index = 0;
    float angle = 2.0f * M_PI * index / num_segments;

    GamepadInputs inputs;

    // joystick inputs are in the range [-1, 1] float
    inputs.left_joystick.x = sin(angle);
    inputs.left_joystick.y = cos(angle);

    inputs.right_joystick.x = cos(angle);
    inputs.right_joystick.y = sin(angle);

    index = (index % num_segments) + 1;

    // set the inputs in the usb gamepad
    usb_gamepad->set_gamepad_inputs(inputs);

    // get the output report from the usb gamepad
    auto report = usb_gamepad->get_report_data(1);

    if (tud_mounted()) {
      bool success = tud_hid_report(1, report.data(), report.size());
      espp::Rgb color = success ? espp::Rgb(0.0f, 1.0f, 0.0f) : espp::Rgb(1.0f, 0.0f, 0.0f);
      // toggle the LED each send, so mod 2
      if (index % 2 == 0) {
        bsp.led(color);
      } else {
        bsp.led(espp::Rgb(0.0f, 0.0f, 0.0f));
      }
    }
  }
}
