// Host-side unit test for the device-configuration payload encoding. The
// protocol header is standard-library only, so this builds on a host:
//
//   c++ -std=c++20 -Wall -Wextra -Werror -I components/device_config/include \
//       components/device_config/test/device_config_host_test.cpp -o dc_test && ./dc_test

#include <cstdio>

#include "detail/device_config_protocol.hpp"

namespace dc = device_config;

static int g_failures = 0;
#define CHECK(cond)                                                                                \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                                      \
      ++g_failures;                                                                                \
    }                                                                                              \
  } while (0)

int main() {
  // ---- Settings round trip ----
  dc::Settings s;
  s.invert_left_y = false;
  s.swap_ab = true;
  s.deadzone_percent = 12;
  s.led_brightness = 40;
  s.ble_name = "My Dongle";
  CHECK(s.validate().empty());
  const auto bytes = s.serialize();
  CHECK(bytes[0] == dc::kProtocolVersion);
  CHECK(bytes[1] == 7); // seven TLVs
  const auto back = dc::Settings::parse(bytes, dc::Settings{});
  CHECK(back.has_value());
  CHECK(back && *back == s);

  // ---- partial SET applies over the base ----
  {
    std::vector<uint8_t> partial{dc::kProtocolVersion, 1, uint8_t(dc::Key::Deadzone), 1, 25};
    const auto r = dc::Settings::parse(partial, s);
    CHECK(r && r->deadzone_percent == 25 && r->swap_ab == true && r->ble_name == "My Dongle");
  }
  // ---- unknown key is ignored (forward compatible) ----
  {
    std::vector<uint8_t> unknown{dc::kProtocolVersion,     2, 0x7E, 2, 1, 2,
                                 uint8_t(dc::Key::SwapXY), 1, 1};
    const auto r = dc::Settings::parse(unknown, dc::Settings{});
    CHECK(r && r->swap_xy == true);
  }
  // ---- malformed: truncated TLV, wrong length for a known key, empty ----
  {
    std::vector<uint8_t> truncated{dc::kProtocolVersion, 1, uint8_t(dc::Key::Deadzone), 4, 1};
    CHECK(!dc::Settings::parse(truncated, dc::Settings{}).has_value());
    std::vector<uint8_t> wrong_len{dc::kProtocolVersion, 1, uint8_t(dc::Key::SwapAB), 2, 1, 1};
    CHECK(!dc::Settings::parse(wrong_len, dc::Settings{}).has_value());
    CHECK(!dc::Settings::parse({}, dc::Settings{}).has_value());
  }
  // ---- validation ----
  {
    dc::Settings bad;
    bad.deadzone_percent = dc::kMaxDeadzonePercent + 1;
    CHECK(!bad.validate().empty());
    bad = dc::Settings{};
    bad.led_brightness = 101;
    CHECK(!bad.validate().empty());
    bad = dc::Settings{};
    bad.ble_name = "";
    CHECK(!bad.validate().empty());
    bad.ble_name = std::string(dc::kMaxBleNameLength + 1, 'a');
    CHECK(!bad.validate().empty());
    bad.ble_name = "tab\there";
    CHECK(!bad.validate().empty());
    CHECK(dc::Settings{}.validate().empty()); // defaults are valid
  }

  // ---- Info round trip ----
  dc::Info info;
  info.project = "esp-usb-ble-hid";
  info.firmware = "v2.1.0-3-gabc";
  info.hardware = "LilyGo T-Dongle-S3";
  info.idf_version = "v6.1";
  info.controller = "0123456789";
  info.uptime_s = 0x01020304;
  info.battery_percent = 87;
  info.bond_count = 2;
  info.usb_mounted = true;
  info.ble_connected = true;
  info.pairing = true;
  const auto ib = info.serialize();
  CHECK(ib[0] == dc::kProtocolVersion);
  CHECK(ib[1] ==
        (dc::Info::kFlagUsbMounted | dc::Info::kFlagBleConnected | dc::Info::kFlagPairing));
  CHECK(ib[4] == 0x04 && ib[5] == 0x03 && ib[6] == 0x02 && ib[7] == 0x01); // uptime LE
  const auto iback = dc::Info::parse(ib);
  CHECK(iback.has_value());
  if (iback) {
    CHECK(iback->project == info.project && iback->firmware == info.firmware);
    CHECK(iback->hardware == info.hardware && iback->idf_version == info.idf_version);
    CHECK(iback->controller == info.controller && iback->uptime_s == info.uptime_s);
    CHECK(iback->battery_percent == 87 && iback->bond_count == 2);
    CHECK(iback->usb_mounted && iback->ble_connected && !iback->ble_scanning && iback->pairing);
  }
  CHECK(!dc::Info::parse(std::span<const uint8_t>(ib.data(), ib.size() - 1)).has_value());

  // ---- OK / ERROR payloads ----
  CHECK(dc::make_ok_payload(dc::Msg::SetSettings) == std::vector<uint8_t>{0x03});
  const auto err = dc::make_error_payload(dc::Msg::Action, dc::ErrorCode::Failed, "nope");
  CHECK(err.size() == 1 + 4 + 4);
  const auto perr = dc::parse_error_payload(err);
  CHECK(perr && perr->request == dc::Msg::Action && perr->code == dc::ErrorCode::Failed &&
        perr->message == "nope");
  CHECK(!dc::parse_error_payload(std::vector<uint8_t>{0x05, 1, 0}).has_value());

  if (g_failures == 0) {
    std::printf("ALL DEVICE CONFIG PROTOCOL TESTS PASSED\n");
    return 0;
  }
  std::printf("%d FAILURE(S)\n", g_failures);
  return 1;
}
