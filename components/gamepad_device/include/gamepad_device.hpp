#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base_component.hpp"
#include "gamepad_inputs.hpp"

struct DeviceInfo {
  uint16_t vid;
  uint16_t pid;
  uint16_t bcd{0x0100};
  uint16_t usb_bcd{0x0200};
  std::string manufacturer_name;
  std::string product_name;
  std::string serial_number;
};

class GamepadDevice : public espp::BaseComponent {
public:
  explicit GamepadDevice(const std::string &name)
      : BaseComponent(name) {}

  typedef std::pair<uint8_t, std::vector<uint8_t>> ReportData;

  // Info
  virtual const DeviceInfo &get_device_info() const = 0;

  // Report Data
  virtual uint8_t get_input_report_id() const { return 0; }
  virtual std::vector<uint8_t> get_report_descriptor() const { return {}; }
  virtual void set_report_data(uint8_t report_id, const uint8_t *data, size_t len) {}
  virtual std::vector<uint8_t> get_report_data(uint8_t report_id) const { return {}; }

  // Gamepad inputs
  virtual GamepadInputs get_gamepad_inputs() const { return {}; }
  virtual void set_gamepad_inputs(const GamepadInputs &inputs) {}

  // HID handlers
  virtual std::optional<ReportData> on_attach() { return {}; }
  virtual std::optional<ReportData> on_hid_report(uint8_t report_id, const uint8_t *data,
                                                  size_t len) {
    return {};
  }
}; // GamepadDevice
