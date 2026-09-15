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
    // booleans are exactly 0 or 1: any other byte is malformed, not "true"
    std::vector<uint8_t> bad_bool{dc::kProtocolVersion, 1, uint8_t(dc::Key::SwapAB), 1, 2};
    CHECK(!dc::Settings::parse(bad_bool, dc::Settings{}).has_value());
    std::vector<uint8_t> ok_bool{dc::kProtocolVersion, 1, uint8_t(dc::Key::SwapAB), 1, 1};
    CHECK(dc::Settings::parse(ok_bool, dc::Settings{}).has_value() &&
          dc::Settings::parse(ok_bool, dc::Settings{})->swap_ab);
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
  // link diagnostics trailer: round trip, and an old-format payload (no
  // trailer) still parses with the "unknown" defaults
  {
    dc::Info d = info;
    d.ble_notifications = 12345;
    d.ble_last_notify_age_ms = 250;
    d.usb_hid_reports = 999999;
    d.usb_hid_ready = true;
    const auto db = d.serialize();
    CHECK(db.size() == ib.size()); // same layout, different values
    CHECK(db[1] & dc::Info::kFlagUsbHidReady);
    const auto dback = dc::Info::parse(db);
    CHECK(dback && dback->ble_notifications == 12345 && dback->ble_last_notify_age_ms == 250 &&
          dback->usb_hid_reports == 999999 && dback->usb_hid_ready);
    const auto old = dc::Info::parse(std::span<const uint8_t>(ib.data(), ib.size() - 12));
    CHECK(old && old->ble_notifications == 0 && old->ble_last_notify_age_ms == dc::Info::kNever &&
          old->usb_hid_reports == 0 && !old->usb_hid_ready && old->controller == info.controller);
  }
  CHECK(!dc::Info::parse(std::span<const uint8_t>(ib.data(), ib.size() - 13)).has_value());

  // ---- version gating: a different payload version is refused, not mis-decoded ----
  {
    auto v2 = s.serialize();
    v2[0] = dc::kProtocolVersion + 1;
    CHECK(!dc::Settings::parse(v2, dc::Settings{}).has_value());
    auto i2 = info.serialize();
    i2[0] = dc::kProtocolVersion + 1;
    CHECK(!dc::Info::parse(i2).has_value());
  }

  // ---- Bonds round trip ----
  {
    std::vector<dc::BondInfo> bonds(2);
    bonds[0].address = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    bonds[0].address_type = 1;
    bonds[0].connected = true;
    bonds[0].label = "0123456789";
    bonds[1].address = {0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99};
    CHECK(bonds[0].address_string() == "11:22:33:44:55:66");
    const auto bb = dc::serialize_bonds(bonds);
    CHECK(bb[0] == dc::kProtocolVersion && bb[1] == 2);
    CHECK(bb.size() == 2 + 2 * (6 + 1 + 1 + 1) + 10);
    const auto back_bonds = dc::parse_bonds(bb);
    CHECK(back_bonds.has_value() && *back_bonds == bonds);
    CHECK(!dc::parse_bonds(std::span<const uint8_t>(bb.data(), bb.size() - 1)).has_value());
    CHECK(dc::parse_bonds(dc::serialize_bonds({})).has_value() &&
          dc::parse_bonds(dc::serialize_bonds({}))->empty());
    // FORGET_BOND payload
    const auto fb = dc::make_forget_bond_payload(bonds[1].address, 0);
    CHECK(fb.size() == 7);
    const auto pf = dc::parse_forget_bond_payload(fb);
    CHECK(pf && pf->first == bonds[1].address && pf->second == 0);
    CHECK(!dc::parse_forget_bond_payload(std::vector<uint8_t>{1, 2, 3}).has_value());
  }

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
