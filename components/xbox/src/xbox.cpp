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
  input_report.set_data(data_vec);
}

std::vector<uint8_t> Xbox::get_report_data(uint8_t report_id) const {
  return input_report.get_report();
}

// Gamepad inputs
GamepadInputs Xbox::get_gamepad_inputs() const {
  GamepadInputs inputs{};
  return inputs;
}

void Xbox::set_gamepad_inputs(const GamepadInputs &inputs) {}

// HID handlers
std::optional<GamepadDevice::ReportData> Xbox::on_attach() { return {}; }

std::optional<GamepadDevice::ReportData> Xbox::on_hid_report(uint8_t report_id, const uint8_t *data,
                                                             size_t len) {
  return {};
}
