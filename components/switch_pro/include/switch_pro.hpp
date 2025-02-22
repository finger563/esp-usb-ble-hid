#pragma once

#include <string>
#include <vector>

#include "hid-rp-switch-pro.hpp"
#include "range_mapper.hpp"

#include "gamepad_device.hpp"

class SwitchPro : public GamepadDevice {
public:
  // Constructor
  explicit SwitchPro()
      : GamepadDevice("SwitchPro")
      , thumbstick_range_mapper({.center = InputReport::joystick_center,
                                 .minimum = InputReport::joystick_min,
                                 .maximum = InputReport::joystick_max}) {}

  // Info
  virtual const DeviceInfo &get_device_info() const override { return device_info; }

  // Report Data
  virtual uint8_t get_input_report_id() const override { return InputReport::ID; }
  virtual std::vector<uint8_t> get_report_descriptor() const override {
    return std::vector<uint8_t>(report_descriptor.begin(), report_descriptor.end());
  }
  virtual void set_report_data(uint8_t report_id, const uint8_t *data, size_t len) override;
  virtual std::vector<uint8_t> get_report_data(uint8_t report_id) const override;

  // Gamepad inputs
  virtual GamepadInputs get_gamepad_inputs() const override;
  virtual void set_gamepad_inputs(const GamepadInputs &inputs) override;

  // HID handlers
  virtual std::optional<ReportData> on_attach() override;
  virtual std::optional<ReportData> on_hid_report(uint8_t report_id, const uint8_t *data,
                                                  size_t len) override;

protected:
  static constexpr auto report_descriptor = espp::switch_pro_descriptor();

  static constexpr uint16_t usb_bcd = 0x0200;
  static constexpr uint16_t vid = 0x057E;
  static constexpr uint16_t pid = 0x2009;
  static constexpr uint16_t bcd = 0x0200;

  static constexpr const char manufacturer[] = "Nintendo Co., Ltd.";
  static constexpr const char product[] = "Pro Controller";
  static constexpr const char serial[] = "000000000001";

  static const DeviceInfo device_info;

  espp::FloatRangeMapper thumbstick_range_mapper;

  using InputReport = espp::SwitchProGamepadInputReport<>;
  InputReport input_report;
}; // class SwitchPro
