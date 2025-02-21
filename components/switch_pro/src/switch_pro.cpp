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
  input_report.set_data(data_vec);
}

std::vector<uint8_t> SwitchPro::get_report_data(uint8_t report_id) const {
  return input_report.get_report();
}

// Gamepad inputs
GamepadInputs SwitchPro::get_gamepad_inputs() const {
  GamepadInputs inputs{};
  return inputs;
}

void SwitchPro::set_gamepad_inputs(const GamepadInputs &inputs) {}

// HID handlers
std::optional<GamepadDevice::ReportData> SwitchPro::on_attach() { return {}; }

std::optional<GamepadDevice::ReportData> SwitchPro::on_hid_report(uint8_t report_id,
                                                                  const uint8_t *data, size_t len) {
  return {};
}
