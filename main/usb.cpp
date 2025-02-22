#include "usb.hpp"

static espp::Logger logger({.tag = "USB"});
static std::shared_ptr<GamepadDevice> usb_gamepad;

/************* TinyUSB descriptors ****************/

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_INOUT_DESC_LEN)
static_assert(CFG_TUD_HID >= 1, "CFG_TUD_HID must be at least 1");

static std::vector<uint8_t> hid_report_descriptor;

static tusb_desc_device_t desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                         .bDescriptorType = TUSB_DESC_DEVICE,
                                         .bcdUSB = 0, // NOTE: to be filled out later
                                         .bDeviceClass = 0,
                                         .bDeviceSubClass = 0,
                                         .bDeviceProtocol = 0,

                                         .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

                                         .idVendor = 0,  // NOTE: to be filled out later
                                         .idProduct = 0, // NOTE: to be filled out later
                                         .bcdDevice = 0, // NOTE: to be filled out later

                                         // Index of manufacturer description string
                                         .iManufacturer = 0x01,
                                         // Index of product description string
                                         .iProduct = 0x02,
                                         // Index of serial number description string
                                         .iSerialNumber = 0x03,
                                         // Number of configurations
                                         .bNumConfigurations = 0x01};

static const char *hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Finger563",          // 1: Manufacturer, NOTE: to be filled out later
    "USB BLE Dongle",     // 2: Product, NOTE: to be filled out later
    "20011201",           // 3: Serials, NOTE: to be filled out later
    "USB HID Interface",  // 4: HID
};

void start_usb_gamepad(const std::shared_ptr<GamepadDevice> &gamepad_device) {
  // store the gamepad device
  usb_gamepad = gamepad_device;

  // update the usb descriptors
  const auto &device_info = usb_gamepad->get_device_info();
  hid_string_descriptor[1] = device_info.manufacturer_name.c_str();
  hid_string_descriptor[2] = device_info.product_name.c_str();
  hid_string_descriptor[3] = device_info.serial_number.c_str();
  desc_device.idVendor = device_info.vid;
  desc_device.idProduct = device_info.pid;
  desc_device.bcdDevice = device_info.bcd;
  desc_device.bcdUSB = device_info.usb_bcd;

  // update the report descriptor
  hid_report_descriptor = usb_gamepad->get_report_descriptor();

  // update the configuration descriptor with the new report descriptor size
  uint8_t hid_configuration_descriptor[] = {
      // Configuration number, interface count, string index, total length, attribute, power in mA
      TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, 0x00, 100),

      // Interface number, string index, boot protocol, report descriptor len, EP In address, size &
      // polling interval
      TUD_HID_INOUT_DESCRIPTOR(0, 4, false, hid_report_descriptor.size(), 0x01, 0x81,
                               CFG_TUD_HID_EP_BUFSIZE, 1),
  };

  const tinyusb_config_t tusb_cfg = {.device_descriptor = &desc_device,
                                     .string_descriptor = hid_string_descriptor,
                                     .string_descriptor_count = sizeof(hid_string_descriptor) /
                                                                sizeof(hid_string_descriptor[0]),
                                     .external_phy = false,
                                     .configuration_descriptor = hid_configuration_descriptor,
                                     .self_powered = false};

  if (tinyusb_driver_install(&tusb_cfg)) {
    logger.error("Failed to install tinyusb driver");
    return;
  }
  logger.info("USB initialization DONE");
}

void stop_usb_gamepad() {}

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to
// complete
extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
  return hid_report_descriptor.data();
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                          hid_report_type_t report_type, uint8_t *buffer,
                                          uint16_t reqlen) {
  switch (report_type) {
  case HID_REPORT_TYPE_INPUT: {
    auto report_data = usb_gamepad->get_report_data(report_id);
    std::copy(report_data.begin(), report_data.end(), buffer);
    return report_data.size();
  }
  default:
    return 0;
  }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type, uint8_t const *buffer,
                                      uint16_t bufsize) {
  // Pass along the report to the gamepad device and send the response back to the host
  if (report_type == HID_REPORT_TYPE_OUTPUT) {
    // pass the report along to the currently configured usb gamepad device
    auto maybe_response = usb_gamepad->on_hid_report(report_id, buffer, bufsize);
    if (maybe_response.has_value()) {
      auto &[report_id, report_data] = maybe_response.value();
      // send the response back to the host
      tud_hid_report(report_id, report_data.data(), report_data.size());
    }
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
extern "C" void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *reprot, uint16_t len) {
  // TODO: debug this
}
