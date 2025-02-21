#pragma once

#include <string>
#include <vector>

#include "gamepad_device.hpp"
#include "hid-rp-xbox.hpp"

class Xbox : public GamepadDevice {
public:
  // Constructor
  explicit Xbox()
      : GamepadDevice("Xbox") {}

  // Info
  virtual const DeviceInfo &get_device_info() const override { return device_info; }

  // Report Data
  virtual std::vector<uint8_t> get_report_descriptor() const override {
    return std::vector<uint8_t>(report_descriptor.begin(), report_descriptor.end());
  }
  virtual void set_report_data(uint8_t report_id, const uint8_t *data, size_t len) override;
  std::vector<uint8_t> get_report_data(uint8_t report_id) const override;

  // Gamepad inputs
  virtual GamepadInputs get_gamepad_inputs() const override;
  virtual void set_gamepad_inputs(const GamepadInputs &inputs) override;

  // HID handlers
  virtual std::optional<ReportData> on_attach() override;
  virtual std::optional<ReportData> on_hid_report(uint8_t report_id, const uint8_t *data,
                                                  size_t len) override;

protected:
  static constexpr auto report_descriptor = espp::xbox_descriptor();

  static constexpr uint16_t usb_bcd = 0x0200;
  static constexpr uint16_t vid = 0x045E;
  // static constexpr uint16_t pid = 0x0B13; // XB Wireless Controller (model 1708)
  static constexpr uint16_t pid = 0x028E; // Xbox 360 Wired
  static constexpr uint16_t bcd = 0x0110;

  static constexpr const char manufacturer[] = "Microsoft";
  static constexpr const char product[] = "Controller";
  static constexpr const char serial[] = "1337";

  static const DeviceInfo device_info;

  using InputReport = espp::XboxGamepadInputReport<>;
  InputReport input_report;
  static constexpr uint8_t input_report_id = InputReport::ID;
  static constexpr uint8_t num_buttons = InputReport::button_count;

  using BatteryReport = espp::XboxBatteryInputReport<>;
  static BatteryReport battery_report;
  static constexpr uint8_t battery_report_id = BatteryReport::ID;

  using RumbleReport = espp::XboxRumbleOutputReport<>;
  static RumbleReport rumble_report;
  static constexpr uint8_t rumble_report_id = RumbleReport::ID;
}; // class SwitchPro
