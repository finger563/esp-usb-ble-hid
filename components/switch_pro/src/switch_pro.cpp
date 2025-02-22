#include "switch_pro.hpp"

const DeviceInfo SwitchPro::device_info = {.vid = SwitchPro::vid,
                                           .pid = SwitchPro::pid,
                                           .bcd = SwitchPro::bcd,
                                           .usb_bcd = SwitchPro::usb_bcd,
                                           .manufacturer_name = SwitchPro::manufacturer,
                                           .product_name = SwitchPro::product,
                                           .serial_number = SwitchPro::serial};

void SwitchPro::set_report_data(uint8_t report_id, const uint8_t *data, size_t len) {
  std::vector<uint8_t> data_vec(data, data + len);
  switch (report_id) {
  case input_report.ID:
    input_report.set_data(data_vec);
    break;
  default:
    logger_.warn("Unknown report id: {}", report_id);
    break;
  }
}

std::vector<uint8_t> SwitchPro::get_report_data(uint8_t report_id) const {
  switch (report_id) {
  case input_report.ID:
    return input_report.get_report();
  default:
    return {};
  }
}

// Gamepad inputs
GamepadInputs SwitchPro::get_gamepad_inputs() const {
  GamepadInputs inputs{};
  input_report.get_buttons(inputs.buttons);
  input_report.get_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report.get_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report.get_brake(inputs.l2.value);
  input_report.get_accelerator(inputs.r2.value);

  return inputs;
}

void SwitchPro::set_gamepad_inputs(const GamepadInputs &inputs) {
  input_report.reset();

  input_report.set_buttons(inputs.buttons);
  input_report.set_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report.set_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report.set_brake(inputs.l2.value);
  input_report.set_accelerator(inputs.r2.value);
}

// HID handlers
std::optional<GamepadDevice::ReportData> SwitchPro::on_attach() { return {}; }

std::optional<GamepadDevice::ReportData> SwitchPro::on_hid_report(uint8_t report_id,
                                                                  const uint8_t *data, size_t len) {
  return {};
}
