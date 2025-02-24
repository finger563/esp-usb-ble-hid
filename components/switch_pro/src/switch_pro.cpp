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
  case input_report_.ID:
    input_report_.set_data(data_vec);
    break;
  default:
    logger_.warn("Unknown report id: {}", report_id);
    break;
  }
}

std::vector<uint8_t> SwitchPro::get_report_data(uint8_t report_id) const {
  if (!hid_ready_) {
    return {};
  }
  switch (report_id) {
  case input_report_.ID:
    return input_report_.get_report();
  default:
    return {};
  }
}

// Gamepad inputs
GamepadInputs SwitchPro::get_gamepad_inputs() const {
  GamepadInputs inputs{};
  input_report_.get_buttons(inputs.buttons);
  input_report_.get_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report_.get_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report_.get_brake(inputs.l2.value);
  input_report_.get_accelerator(inputs.r2.value);

  return inputs;
}

void SwitchPro::set_gamepad_inputs(const GamepadInputs &inputs) {
  input_report_.reset();

  input_report_.set_buttons(inputs.buttons);
  input_report_.set_left_joystick(inputs.left_joystick.x, inputs.left_joystick.y);
  input_report_.set_right_joystick(inputs.right_joystick.x, inputs.right_joystick.y);
  input_report_.set_brake(inputs.l2.value);
  input_report_.set_accelerator(inputs.r2.value);

  // set housekeeping data
  input_report_.set_usb_powered(true);
  // static constexpr uint8_t PRO_CONTROLLER = (0 << 1); // b00 << 1 = Pro Controller, b11 << 1 =
  // Joy-Con static constexpr uint8_t SWITCH_POWERED = 0b1; // b1 = Switch powered, b0 = not powered
  input_report_.set_connection_info(sp::PRO_CONTROLLER.connection_info);
}

// HID handlers
std::optional<GamepadDevice::ReportData> SwitchPro::on_attach() {
  using namespace sp;
  // return data to start the initialization sequence
  // Kick off initialization sequence by providing device info
  return {{DEVICE_INIT_REPORT,
           std::vector<uint8_t>(device_init_report_data,
                                device_init_report_data + sizeof(device_init_report_data))}};
}

std::optional<GamepadDevice::ReportData> SwitchPro::on_hid_report(uint8_t report_id,
                                                                  const uint8_t *data, size_t len) {
  // ignore the report_id
  (void)report_id;

  using namespace sp;

  switch (data[0]) {
  case HOST_INIT_REPORT: {
    uint8_t cmd = data[1];
    std::vector<uint8_t> resp(63, 0);
    resp[0] = cmd;
    switch (cmd) {
    case INIT_COMMAND_DEVICE_INFO:
      break;
    case INIT_COMMAND_HANDSHAKE:
      // copy the input data back into the response
      std::copy(data + 1, data + len, resp.begin());
      break;
    case INIT_COMMAND_SET_BAUD_RATE:
      break;
    case INIT_COMMAND_ENABLE_USB_HID:
      // Ok to start sending input reports
      hid_ready_ = true;
      break;
    case INIT_COMMAND_ENABLE_BT_HID:
      // TODO: we should disable USB input reports, but probably doesn't matter
      break;
    }
    return {{DEVICE_INIT_REPORT, resp}};
  }
  case HOST_OUTPUT_REPORT: {
    return process_command(data, len);
  }
  case HOST_RUMBLE_REPORT: {
    return {};
  }
  default:
    break;
  }

  return {};
}
