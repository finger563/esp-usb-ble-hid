#pragma once

#include <string>
#include <vector>

#include "hid-rp-switch-pro.hpp"
#include "range_mapper.hpp"

#include "gamepad_device.hpp"
#include "timer.hpp"

#include "switch_controller_protocol.hpp"

class SwitchPro : public GamepadDevice {
public:
  // Constructor
  explicit SwitchPro()
      : GamepadDevice("SwitchPro")
      , thumbstick_range_mapper_({.center = InputReport::joystick_center,
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

  static constexpr uint16_t usb_bcd = 0x0100;
  static constexpr uint16_t vid = 0x057E;
  static constexpr uint16_t pid = 0x2009;
  static constexpr uint16_t bcd = 0x0200;

  static constexpr const char manufacturer[] = "Nintendo Co., Ltd.";
  static constexpr const char product[] = "Pro Controller";
  static constexpr const char serial[] = "000000000001";

  GamepadDevice::ReportData process_command(const uint8_t *data, size_t len);
  void set_subcommand_reply(std::vector<uint8_t> &report);
  void set_unknown_subcommand(std::vector<uint8_t> &report, uint8_t subcommand_id);
  void set_full_input_report(std::vector<uint8_t> &report);
  void set_standard_input_report(std::vector<uint8_t> &report);
  void set_device_info(std::vector<uint8_t> &report);
  void set_shipment(std::vector<uint8_t> &report);
  void toggle_imu(std::vector<uint8_t> &report, sp::Message &message);
  void set_imu_data(std::vector<uint8_t> &report);
  void spi_read(std::vector<uint8_t> &report, sp::Message &message);
  void set_mode(std::vector<uint8_t> &report, sp::Message &message);
  void set_trigger_buttons(std::vector<uint8_t> &report);
  void enable_vibration(std::vector<uint8_t> &report);
  void set_player_lights(std::vector<uint8_t> &report, sp::Message &message);
  void set_nfc_ir_state(std::vector<uint8_t> &report);
  void set_nfc_ir_config(std::vector<uint8_t> &report);

  static const DeviceInfo device_info;

  espp::FloatRangeMapper thumbstick_range_mapper_;

  bool hid_ready_ = false; // set after device info has been queried

  uint8_t input_report_mode_ = 0; // standard (0x30), nfc/ir (0x31), simpleHID (0x3F)
  uint8_t player_number_ = 0;     // valid values are 1, 2, 3, and 4
  bool vibration_enabled_ = false;
  uint8_t vibrator_report_{0}; // randomly selected from sp::vibrator_bytes
  bool imu_enabled_ = false;
  uint8_t input_report_id_ = 0x21;
  sp::TriggerTimes trigger_times_{};

  using InputReport = espp::SwitchProGamepadInputReport<>;
  InputReport input_report_;

  espp::Timer counter_timer_{{
      .name = "Switch Pro Counter Timer",
      .period = std::chrono::milliseconds(5), // Joy-Con uses 4.96ms as the timer tick rate
      .callback =
          [this]() {
            input_report_.increment_counter();
            return false; // do not stop the timer
          },
  }};
}; // class SwitchPro
