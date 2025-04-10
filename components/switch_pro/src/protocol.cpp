#include "switch_pro.hpp"
#include "switch_pro_spi_rom_data.hpp"

#if defined(ESP_PLATFORM)
#include <esp_random.h>
#else
#include <random>
#endif

using namespace sp;

// SWITCH blocks on 0x81 0x01 and 0x21 0x03

static void replace_subarray(std::vector<uint8_t> &arr, size_t start, size_t end,
                             const uint8_t *replace_arr) {
  for (size_t i = start; i < end; i++) {
    arr[i] = replace_arr[i - start];
  }
}

// credits to
// https://github.com/Brikwerk/nxbt/blob/master/nxbt/controller/protocol.py for
// the best protocol implementation I could find for Joycon / Switch Pro
// controllers.

GamepadDevice::ReportData SwitchPro::process_command(const uint8_t *data, size_t len) {
  // Parsing the Switch's message
  Message message(data, len);

  // prep most common response, which contains the full input report
  std::vector<uint8_t> report;

  {
    std::lock_guard<std::recursive_mutex> lock(input_report_mutex_);
    report = input_report_.get_report();
  }

  report[12] = 0x80;
  report[13] = message.subcommand_id;

  // for sanity, go ahead and set the next byte to 0
  report[14] = 0;

  // Responding to the parsed message
  if (message.response == Response::ONLY_CONTROLLER_STATE) {
    set_subcommand_reply(report);
    // ACK byte
    report[12] = 0x80;
    // Subcommand reply
    report[13] = 0x00;
  } else if (message.response == Response::BT_MANUAL_PAIRING) {
    set_subcommand_reply(report);
    // ACK byte
    report[12] = 0x81;
    // Subcommand reply
    report[13] = 0x01;
  } else if (message.response == Response::REQUEST_DEVICE_INFO) {
    hid_ready_ = true;
    set_subcommand_reply(report);
    set_device_info(report);
  } else if (message.response == Response::SET_SHIPMENT) {
    set_subcommand_reply(report);
    set_shipment(report);
  } else if (message.response == Response::SPI_READ) {
    set_subcommand_reply(report);
    spi_read(report, message);
  } else if (message.response == Response::SET_MODE) {
    set_subcommand_reply(report);
    set_mode(report, message);
  } else if (message.response == Response::TRIGGER_BUTTONS_ELAPSED) {
    set_subcommand_reply(report);
    set_trigger_buttons(report);
  } else if (message.response == Response::TOGGLE_IMU) {
    set_subcommand_reply(report);
    toggle_imu(report, message);
  } else if (message.response == Response::ENABLE_VIBRATION) {
    set_subcommand_reply(report);
    enable_vibration(report);
  } else if (message.response == Response::SET_PLAYER) {
    set_subcommand_reply(report);
    set_player_lights(report, message);
  } else if (message.response == Response::SET_NFC_IR_STATE) {
    set_subcommand_reply(report);
    set_nfc_ir_state(report);
  } else if (message.response == Response::SET_NFC_IR_CONFIG) {
    set_subcommand_reply(report);
    set_nfc_ir_config(report);
    // Bad Packet handling statements
  } else if (message.response == Response::UNKNOWN_SUBCOMMAND) {
    // Currently set so that the controller ignores any unknown
    // subcommands. This is better than sending a NACK response
    // since we'd just get stuck in an infinite loop arguing
    // with the Switch.
    // set_full_input_report(report);
    set_unknown_subcommand(report, message.subcommand_id);
  } else if (message.response == Response::NO_DATA) {
    set_unknown_subcommand(report, message.subcommand_id);
    // set_full_input_report(report);
  } else if (message.response == Response::TOO_SHORT) {
    set_unknown_subcommand(report, message.subcommand_id);
    // set_full_input_report(report);
  } else if (message.response == Response::MALFORMED) {
    set_unknown_subcommand(report, message.subcommand_id);
    // set_full_input_report(report);
  }

  return {input_report_id_, report};
}

void SwitchPro::set_subcommand_reply(std::vector<uint8_t> &report) {
  // Input Report ID
  input_report_id_ = 0x21;

  // TODO: Find out what the vibrator byte is doing.
  // This is a hack in an attempt to semi-emulate
  // actions of the vibrator byte as it seems to change
  // when a subcommand reply is sent.
  // vibrator_report_ = random.choice(VIBRATOR_BYTES);

  // set vibrator_report_ to be a randomly chosen element of the
  // sp::vibrator_bytes array
  std::size_t max_index = sizeof(sp::vibrator_bytes) / sizeof(sp::vibrator_bytes[0]);
#if defined(ESP_PLATFORM)
  vibrator_report_ = sp::vibrator_bytes[esp_random() % max_index];
#else
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis(0, max_index - 1);
  vibrator_report_ = sp::vibrator_bytes[dis(gen)];
#endif

  set_standard_input_report(report);
}

void SwitchPro::set_unknown_subcommand(std::vector<uint8_t> &report, uint8_t subcommand_id) {
  // Set ACK
  report[12] = 0x80;
  // Set unknown subcommand ID
  report[13] = subcommand_id;
  // Set unknown subcommand reply
  report[14] = 0x03;
}

void SwitchPro::set_full_input_report(std::vector<uint8_t> &report) {
  // Setting Report ID to full standard input report ID
  input_report_id_ = 0x30;
  set_standard_input_report(report);
  set_imu_data(report);
}

void SwitchPro::set_standard_input_report(std::vector<uint8_t> &report) {
  // set the timer regardless
  {
    std::lock_guard<std::recursive_mutex> lock(input_report_mutex_);
    report[0] = input_report_.get_counter();
  }
  if (hid_ready_) {
    // do nothing, we started off with the correct values. all we have to do is
    // set the vibrator byte
    report[11] = vibrator_report_;
  } else {
    // TODO: should we clear out the report data corresponding to the
    // gamepad (bytes 1-11) here?
  }
}

void SwitchPro::set_device_info(std::vector<uint8_t> &report) {
  // ACK Reply
  report[12] = 0x82;
  // Subcommand Reply
  report[13] = 0x02;

  // copy the device info data into the report
  replace_subarray(report, 14, 14 + sizeof(sp::device_info), sp::device_info);

  // copy BT mac address from mac_address_ into bytes 18 - 23
  std::memcpy(report.data() + 18, mac_address_.data(), mac_address_.size());
}

void SwitchPro::set_shipment(std::vector<uint8_t> &report) {
  // ACK Reply
  report[12] = 0x80;

  // Subcommand reply
  report[13] = 0x08;
}

void SwitchPro::toggle_imu(std::vector<uint8_t> &report, sp::Message &message) {
  if (message.subcommand[1] == 0x01)
    imu_enabled_ = true;
  else
    imu_enabled_ = false;

  // ACK Reply
  report[12] = 0x80;

  // Subcommand reply
  report[13] = 0x40;
}

void SwitchPro::set_imu_data(std::vector<uint8_t> &report) {

  if (!imu_enabled_)
    return;

  static constexpr uint8_t imu_data[] = {0x75, 0xFD, 0xFD, 0xFF, 0x09, 0x10, 0x21, 0x00, 0xD5,
                                         0xFF, 0xE0, 0xFF, 0x72, 0xFD, 0xF9, 0xFF, 0x0A, 0x10,
                                         0x22, 0x00, 0xD5, 0xFF, 0xE0, 0xFF, 0x76, 0xFD, 0xFC,
                                         0xFF, 0x09, 0x10, 0x23, 0x00, 0xD5, 0xFF, 0xE0, 0xFF};
  replace_subarray(report, 12, 49, imu_data);
}

uint8_t SwitchPro::spi_read_impl(uint8_t bank, uint8_t reg, uint8_t read_length,
                                 uint8_t *response) {
  using namespace sp;
  if (bank == REG_BANK_SHIPMENT) {
    // set the byte(s) to 0
    for (int i = 0; i < read_length; i++) {
      response[i] = 0;
    }
  } else if (bank == REG_BANK_FACTORY_CONFIG) {
    // copy from sp::spi_rom_data_60 variable
    std::memcpy(response, spi_rom_factory_data.begin() + reg, read_length);
    return read_length;
  } else if (bank == REG_BANK_USER_CAL) {
    // copy from sp::spi_rom_data_80 variable
    std::memcpy(response, spi_rom_user_data.begin() + reg, read_length);
    return read_length;
  }
  return 0;
}

void SwitchPro::spi_read(std::vector<uint8_t> &report, sp::Message &message) {
  uint8_t addr_top = message.subcommand[2];
  uint8_t addr_bottom = message.subcommand[1];
  uint8_t read_length = message.subcommand[5];

  // try to read from SPI
  if (spi_read_impl(addr_top, addr_bottom, read_length, report.data() + 19) > 0) {
    // If it succeeded, set the response / SPI header
    // ACK byte
    report[12] = 0x90;
    // Subcommand reply
    report[13] = 0x10;
    // Read address
    report[14] = addr_bottom;
    report[15] = addr_top;
    report[16] = 0;
    report[17] = 0;
    // Read length
    report[18] = read_length;
    return;
  }

  // if we got here, the read failed, so simply NACK it
  report[12] = 0x83;
  report[13] = 0x00;

  return;
}

void SwitchPro::set_mode(std::vector<uint8_t> &report, sp::Message &message) {
  // ACK byte
  report[12] = 0x80;

  // Subcommand reply
  report[13] = 0x03;

  input_report_mode_ = message.subcommand[1]; // 0x30 (standard), 0x31 (nfc/ir), 0x3F (simple)
}

void SwitchPro::set_trigger_buttons(std::vector<uint8_t> &report) {
  // ACK byte
  report[12] = 0x83;

  // Subcommand reply
  report[13] = 0x04;

  // see
  // https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x04-trigger-buttons-elapsed-time
  //
  // Replies with 7 little-endian uint16. The values are in 10ms. They reset by
  // turning off the controller.
  //
  // L,R,ZL,ZR,SL,SR,HOME
  //
  // e.g.
  // Left_trigger_ms = ((byte[1] << 8) | byte[0]) * 10;

  std::memcpy(report.data() + 14, &trigger_times_, sizeof(trigger_times_));
}

void SwitchPro::enable_vibration(std::vector<uint8_t> &report) {
  // ACK Reply
  report[12] = 0x82;

  // Subcommand reply
  report[13] = 0x48;

  // Set class property
  vibration_enabled_ = true;
}

void SwitchPro::set_player_lights(std::vector<uint8_t> &report, sp::Message &message) {
  // ACK byte
  report[12] = 0x80;

  // Subcommand reply
  report[13] = 0x30;

  uint8_t bitfield = message.subcommand[1];

  if (bitfield == 0x01 || bitfield == 0x10) {
    player_number_ = 1;
  } else if (bitfield == 0x03 || bitfield == 0x30) {
    player_number_ = 2;
  } else if (bitfield == 0x07 || bitfield == 0x70) {
    player_number_ = 3;
  } else if (bitfield == 0x0F || bitfield == 0xF0) {
    player_number_ = 4;
  }
}

void SwitchPro::set_nfc_ir_state(std::vector<uint8_t> &report) {
  // ACK byte
  report[12] = 0x80;

  // Subcommand reply
  report[13] = 0x22;
}

void SwitchPro::set_nfc_ir_config(std::vector<uint8_t> &report) {
  // ACK byte
  report[12] = 0xA0;

  // Subcommand reply
  report[13] = 0x21;

  // NFC/IR state data
  static constexpr uint8_t params[] = {0x01, 0x00, 0xFF, 0x00, 0x08, 0x00, 0x1B, 0x01};
  replace_subarray(report, 14, 8, params);
  report[47] = 0xC8;
}
