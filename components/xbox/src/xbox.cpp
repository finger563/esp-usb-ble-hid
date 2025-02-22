#include "xbox.hpp"

const DeviceInfo Xbox::device_info = {.vid = Xbox::vid,
                                      .pid = Xbox::pid,
                                      .bcd = Xbox::bcd,
                                      .usb_bcd = Xbox::usb_bcd,
                                      .manufacturer_name = Xbox::manufacturer,
                                      .product_name = Xbox::product,
                                      .serial_number = Xbox::serial};

void Xbox::set_report_data(uint8_t report_id, const uint8_t *data, size_t len) {
  std::vector<uint8_t> data_vec(data, data + len);
  switch (report_id) {
  case input_report.ID:
    input_report.set_data(data_vec);
    break;
  case rumble_report.ID:
    rumble_report.set_data(data_vec);
    break;
  case battery_report.ID:
    battery_report.set_data(data_vec);
    break;
  default:
    logger_.warn("Unknown report id: {}", report_id);
    break;
  }
}

std::vector<uint8_t> Xbox::get_report_data(uint8_t report_id) const {
  switch (report_id) {
  case input_report.ID:
    return input_report.get_report();
  case rumble_report.ID:
    return rumble_report.get_report();
  case battery_report.ID:
    return battery_report.get_report();
  default:
    return {};
  }
}

// Gamepad inputs
GamepadInputs Xbox::get_gamepad_inputs() const {
  GamepadInputs inputs{};
  input_report.get_buttons(inputs.buttons);

  bool up, down, left, right;
  input_report.get_hat(up, down, left, right);
  inputs.buttons.up = up;
  inputs.buttons.down = down;
  inputs.buttons.left = left;
  inputs.buttons.right = right;

  input_report.get_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report.get_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report.get_brake(inputs.l2.value);
  input_report.get_accelerator(inputs.r2.value);

  return inputs;
}

void Xbox::set_gamepad_inputs(const GamepadInputs &inputs) {
  input_report.reset();

  input_report.set_buttons(inputs.buttons);

  input_report.set_hat(inputs.buttons.up, inputs.buttons.down, inputs.buttons.left,
                       inputs.buttons.right);

  input_report.set_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report.set_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report.set_brake(inputs.l2.value);
  input_report.set_accelerator(inputs.r2.value);
}

// HID handlers
std::optional<GamepadDevice::ReportData> Xbox::on_attach() { return {}; }

std::optional<GamepadDevice::ReportData> Xbox::on_hid_report(uint8_t report_id, const uint8_t *data,
                                                             size_t len) {
  return {};
}
