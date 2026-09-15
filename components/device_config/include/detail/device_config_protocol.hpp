#pragma once

// Device-configuration protocol for the ESP USB BLE HID dongle.
//
// This is a custom module on the espp `dispatcher` (stream_frame framing over the
// USB vendor / WebUSB interface). It lets the browser console (web/dongle_console.html)
// read the device status, read / change / reset the persisted settings, and
// trigger actions (start pairing, clear BLE bonds, reboot).
//
// This header is dependency-free (standard library only) so the payload
// encoding is host-testable (see test/device_config_host_test.cpp). The module
// class that plugs it into the dispatcher lives in ../device_config.hpp.
//
// Wire format (all multi-byte integers little-endian, strings = [len u8][bytes]):
//
//   requests (host -> device)            replies (device -> host, frame reply bit set)
//   0x01 GET_INFO      (no payload)      0x81 INFO      (see Info::serialize)
//   0x02 GET_SETTINGS  (no payload)      0x82 SETTINGS  (see Settings::serialize)
//   0x03 SET_SETTINGS  (Settings TLVs)   0x83 OK        [request u8]
//   0x04 RESET_SETTINGS(no payload)      0x84 ERROR     [request u8][code u32][utf8 message]
//   0x05 ACTION        [action u8]       0x85 BONDS     (see serialize_bonds)
//   0x06 GET_BONDS     (no payload)
//   0x07 FORGET_BOND   [addr 6B][type u8]
//
// SET_SETTINGS carries the same TLV list as SETTINGS and may be *partial*: only
// the keys present are changed. On success the device replies OK and then a
// full SETTINGS so the host always ends with the authoritative values.
// FORGET_BOND replies OK and then a fresh BONDS list.
//
// Versioning: INFO / SETTINGS / BONDS payloads start with kProtocolVersion.
// Within a version, payloads only ever grow compatibly (new TLV keys, which
// receivers ignore); a different version means the layout changed and the
// parsers reject it (nullopt) rather than mis-decode it.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace device_config {

/// Dispatcher module id (outside the ids espp assigns: 0 OTA, 3 telemetry, 4
/// core dump, ...; 0xF0-0xFF reserved).
static constexpr uint8_t kModule = 0x10;
/// Payload schema version carried in INFO / SETTINGS.
static constexpr uint8_t kProtocolVersion = 1;

enum class Msg : uint8_t {
  GetInfo = 0x01,
  GetSettings = 0x02,
  SetSettings = 0x03,
  ResetSettings = 0x04,
  Action = 0x05,
  GetBonds = 0x06,
  ForgetBond = 0x07,
  // replies (request | 0x80)
  Info = 0x81,
  Settings = 0x82,
  Ok = 0x83,
  Error = 0x84,
  Bonds = 0x85,
};

/// Settings TLV keys. Booleans are one byte (0/1).
enum class Key : uint8_t {
  InvertLeftY = 0x01,   ///< u8 bool: invert the left stick Y axis (default 1)
  InvertRightY = 0x02,  ///< u8 bool: invert the right stick Y axis (default 1)
  SwapAB = 0x03,        ///< u8 bool: swap A/B (Xbox-style physical layout on the Switch)
  SwapXY = 0x04,        ///< u8 bool: swap X/Y
  Deadzone = 0x05,      ///< u8 percent 0..kMaxDeadzonePercent: radial stick deadzone
  LedBrightness = 0x06, ///< u8 percent 0..100: status LED brightness
  BleName = 0x07,       ///< string 1..kMaxBleNameLength: BLE device name (applies after reboot)
  LedConnectedBrightness = 0x08, ///< u8 percent 0..100: steady LED level while a controller is
                                 ///< connected, relative to LedBrightness (default 25)
  LedActivityBlink = 0x09,       ///< u8 bool: toggle the LED on every input report instead of
                                 ///< holding it steady (debugging aid; default 0)
};

enum class Action : uint8_t {
  StartPairing = 0x01, ///< enter BLE pairing mode (same as holding the button)
  ClearBonds = 0x02,   ///< forget every paired controller
  Reboot = 0x03,       ///< restart the dongle
};

/// Error codes carried in ERROR replies (the message is authoritative).
enum class ErrorCode : uint32_t {
  Malformed = 1,      ///< payload could not be decoded
  InvalidValue = 2,   ///< a setting is out of range
  UnknownRequest = 3, ///< unknown message type / action
  Failed = 4,         ///< the device could not perform the request
  NotFound = 5,       ///< the referenced bond does not exist
};

static constexpr uint8_t kMaxDeadzonePercent = 50;
static constexpr size_t kMaxBleNameLength = 31;

// ---- little-endian helpers ---------------------------------------------------

inline void put_u8(std::vector<uint8_t> &out, uint8_t v) { out.push_back(v); }
inline void put_u32(std::vector<uint8_t> &out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 24));
}
inline void put_str(std::vector<uint8_t> &out, std::string_view s) {
  const size_t n = std::min<size_t>(s.size(), 255);
  out.push_back(static_cast<uint8_t>(n));
  out.insert(out.end(), s.begin(), s.begin() + n);
}
inline uint32_t get_u32(std::span<const uint8_t> in) {
  return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
         (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

/// A cursor over a payload that fails soft (returns nullopt) instead of reading
/// past the end.
class Reader {
public:
  explicit Reader(std::span<const uint8_t> data)
      : data_(data) {}
  bool done() const { return pos_ >= data_.size(); }
  std::optional<uint8_t> u8() {
    if (pos_ + 1 > data_.size())
      return std::nullopt;
    return data_[pos_++];
  }
  std::optional<uint32_t> u32() {
    if (pos_ + 4 > data_.size())
      return std::nullopt;
    const auto v = get_u32(data_.subspan(pos_, 4));
    pos_ += 4;
    return v;
  }
  std::optional<std::span<const uint8_t>> bytes(size_t n) {
    if (pos_ + n > data_.size())
      return std::nullopt;
    auto s = data_.subspan(pos_, n);
    pos_ += n;
    return s;
  }
  std::optional<std::string> str() {
    const auto n = u8();
    if (!n)
      return std::nullopt;
    const auto b = bytes(*n);
    if (!b)
      return std::nullopt;
    return std::string(b->begin(), b->end());
  }

private:
  std::span<const uint8_t> data_;
  size_t pos_{0};
};

// ---- Settings -----------------------------------------------------------------

/// The persisted, user-editable settings. Defaults match the dongle's original
/// hard-coded behaviour (Y axes inverted for the Switch, no swaps, no deadzone).
struct Settings {
  bool invert_left_y{true};
  bool invert_right_y{true};
  bool swap_ab{false};
  bool swap_xy{false};
  uint8_t deadzone_percent{0};
  uint8_t led_brightness{100};
  std::string ble_name{"Switch"};
  uint8_t led_connected_brightness{25};
  bool led_activity_blink{false};

  bool operator==(const Settings &) const = default;

  /// Range-check every field. Returns an empty string when valid, otherwise a
  /// human-readable reason.
  std::string validate() const {
    if (deadzone_percent > kMaxDeadzonePercent)
      return "deadzone must be 0.." + std::to_string(kMaxDeadzonePercent) + " %";
    if (led_brightness > 100)
      return "led brightness must be 0..100 %";
    if (led_connected_brightness > 100)
      return "led connected brightness must be 0..100 %";
    if (ble_name.empty() || ble_name.size() > kMaxBleNameLength)
      return "ble name must be 1.." + std::to_string(kMaxBleNameLength) + " characters";
    const bool unprintable = std::any_of(ble_name.begin(), ble_name.end(), [](char c) {
      return static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x7F;
    });
    if (unprintable)
      return "ble name must be printable ASCII";
    return {};
  }

  /// SETTINGS payload: [version u8][count u8] then count x [key u8][len u8][value].
  std::vector<uint8_t> serialize() const {
    std::vector<uint8_t> out;
    out.reserve(32 + ble_name.size());
    put_u8(out, kProtocolVersion);
    put_u8(out, 9);
    auto tlv_u8 = [&](Key k, uint8_t v) {
      put_u8(out, static_cast<uint8_t>(k));
      put_u8(out, 1);
      put_u8(out, v);
    };
    tlv_u8(Key::InvertLeftY, invert_left_y ? 1 : 0);
    tlv_u8(Key::InvertRightY, invert_right_y ? 1 : 0);
    tlv_u8(Key::SwapAB, swap_ab ? 1 : 0);
    tlv_u8(Key::SwapXY, swap_xy ? 1 : 0);
    tlv_u8(Key::Deadzone, deadzone_percent);
    tlv_u8(Key::LedBrightness, led_brightness);
    put_u8(out, static_cast<uint8_t>(Key::BleName));
    put_str(out, ble_name);
    tlv_u8(Key::LedConnectedBrightness, led_connected_brightness);
    tlv_u8(Key::LedActivityBlink, led_activity_blink ? 1 : 0);
    return out;
  }

  /// Apply a (possibly partial) SETTINGS / SET_SETTINGS TLV payload on top of
  /// @p base. Unknown keys are ignored (forward compatibility); a malformed
  /// payload (truncated, or a known key with the wrong length) yields nullopt.
  /// The result is NOT range-checked -- call validate() on it.
  static std::optional<Settings> parse(std::span<const uint8_t> payload, const Settings &base) {
    Reader r(payload);
    const auto version = r.u8();
    const auto count = r.u8();
    if (!version || !count)
      return std::nullopt;
    if (*version != kProtocolVersion)
      return std::nullopt; // a different layout: refuse rather than mis-decode
    Settings s = base;
    for (uint8_t i = 0; i < *count; ++i) {
      const auto key = r.u8();
      const auto len = r.u8();
      if (!key || !len)
        return std::nullopt;
      const auto value = r.bytes(*len);
      if (!value)
        return std::nullopt;
      auto as_bool = [&](bool &field) {
        // booleans are exactly 0 or 1 on the wire
        if (value->size() != 1 || (*value)[0] > 1)
          return false;
        field = (*value)[0] == 1;
        return true;
      };
      auto as_u8 = [&](uint8_t &field) {
        if (value->size() != 1)
          return false;
        field = (*value)[0];
        return true;
      };
      bool ok = true;
      switch (static_cast<Key>(*key)) {
      case Key::InvertLeftY:
        ok = as_bool(s.invert_left_y);
        break;
      case Key::InvertRightY:
        ok = as_bool(s.invert_right_y);
        break;
      case Key::SwapAB:
        ok = as_bool(s.swap_ab);
        break;
      case Key::SwapXY:
        ok = as_bool(s.swap_xy);
        break;
      case Key::Deadzone:
        ok = as_u8(s.deadzone_percent);
        break;
      case Key::LedBrightness:
        ok = as_u8(s.led_brightness);
        break;
      case Key::BleName:
        s.ble_name.assign(value->begin(), value->end());
        break;
      case Key::LedConnectedBrightness:
        ok = as_u8(s.led_connected_brightness);
        break;
      case Key::LedActivityBlink:
        ok = as_bool(s.led_activity_blink);
        break;
      default:
        break; // unknown key: ignore
      }
      if (!ok)
        return std::nullopt;
    }
    return s;
  }
};

// ---- Info ----------------------------------------------------------------------

/// Read-only device status returned by GET_INFO.
struct Info {
  std::string project;        ///< app name (from the app description)
  std::string firmware;       ///< firmware version (git describe)
  std::string hardware;       ///< hardware target (e.g. "LilyGo T-Dongle-S3")
  std::string idf_version;    ///< ESP-IDF version the firmware was built with
  std::string controller;     ///< connected controller's serial number ("" if none)
  uint32_t uptime_s{0};       ///< seconds since boot
  uint8_t battery_percent{0}; ///< connected controller's battery (0 if unknown)
  uint8_t bond_count{0};      ///< number of paired (bonded) controllers
  bool usb_mounted{false};    ///< USB host has configured the device
  bool ble_connected{false};  ///< a controller is connected and sending reports
  bool ble_scanning{false};   ///< the dongle is scanning (pairing or reconnecting)
  bool pairing{false};        ///< the scan is a pairing scan (accept new controllers)
  // Link diagnostics (appended in a later firmware; absent = zeros / unknown):
  uint32_t ble_notifications{0};           ///< HID notifications since the controller subscribed
  uint32_t ble_last_notify_age_ms{kNever}; ///< ms since the last one (kNever = none yet)
  uint32_t usb_hid_reports{0};             ///< input reports accepted by the USB stack since boot
  bool usb_hid_ready{false};               ///< the host finished the handshake and takes reports
  uint8_t link_state{0};                   ///< where the controller link is (see LinkState)
  std::string link_detail;                 ///< note on the last link event / problem ("" if none)

  /// The controller link's bring-up step (link_state). 0 is reserved for
  /// firmware that does not report it.
  enum class LinkState : uint8_t {
    Unknown = 0,
    Idle = 1,        ///< not scanning and not connected
    Scanning = 2,    ///< scanning for a bonded (or, when pairing, any) controller
    Connecting = 3,  ///< a connection attempt is in flight
    Encrypting = 4,  ///< connected; waiting for the bond / link encryption
    Subscribing = 5, ///< encrypted; subscribing to the HID input reports
    Subscribed = 6,  ///< subscribed; inputs are expected (ble_connected is set)
  };

  static constexpr uint32_t kNever = 0xFFFFFFFF;
  static constexpr uint8_t kFlagUsbMounted = 0x01;
  static constexpr uint8_t kFlagBleConnected = 0x02;
  static constexpr uint8_t kFlagBleScanning = 0x04;
  static constexpr uint8_t kFlagPairing = 0x08;
  static constexpr uint8_t kFlagUsbHidReady = 0x10;

  /// INFO payload: [version u8][flags u8][battery u8][bonds u8][uptime u32]
  ///               [project str][firmware str][hardware str][idf str][controller str]
  ///               then, appended (older senders stop before it, parsers treat
  ///               it as optional): [ble_notifications u32][ble_last_notify_age_ms u32]
  ///               [usb_hid_reports u32]  (usb_hid_ready is flags bit4), then
  ///               (a later addition, also optional): [link_state u8][link_detail str]
  std::vector<uint8_t> serialize() const {
    std::vector<uint8_t> out;
    put_u8(out, kProtocolVersion);
    uint8_t flags = 0;
    if (usb_mounted)
      flags |= kFlagUsbMounted;
    if (ble_connected)
      flags |= kFlagBleConnected;
    if (ble_scanning)
      flags |= kFlagBleScanning;
    if (pairing)
      flags |= kFlagPairing;
    if (usb_hid_ready)
      flags |= kFlagUsbHidReady;
    put_u8(out, flags);
    put_u8(out, battery_percent);
    put_u8(out, bond_count);
    put_u32(out, uptime_s);
    put_str(out, project);
    put_str(out, firmware);
    put_str(out, hardware);
    put_str(out, idf_version);
    put_str(out, controller);
    put_u32(out, ble_notifications);
    put_u32(out, ble_last_notify_age_ms);
    put_u32(out, usb_hid_reports);
    put_u8(out, link_state);
    put_str(out, link_detail);
    return out;
  }

  static std::optional<Info> parse(std::span<const uint8_t> payload) {
    Reader r(payload);
    const auto version = r.u8();
    const auto flags = r.u8();
    const auto battery = r.u8();
    const auto bonds = r.u8();
    const auto uptime = r.u32();
    if (!version || !flags || !battery || !bonds || !uptime)
      return std::nullopt;
    if (*version != kProtocolVersion)
      return std::nullopt; // a different layout: refuse rather than mis-decode
    Info i;
    i.usb_mounted = *flags & kFlagUsbMounted;
    i.ble_connected = *flags & kFlagBleConnected;
    i.ble_scanning = *flags & kFlagBleScanning;
    i.pairing = *flags & kFlagPairing;
    i.usb_hid_ready = *flags & kFlagUsbHidReady;
    i.battery_percent = *battery;
    i.bond_count = *bonds;
    i.uptime_s = *uptime;
    auto project = r.str(), firmware = r.str(), hardware = r.str(), idf = r.str(),
         controller = r.str();
    if (!project || !firmware || !hardware || !idf || !controller)
      return std::nullopt;
    i.project = *project;
    i.firmware = *firmware;
    i.hardware = *hardware;
    i.idf_version = *idf;
    i.controller = *controller;
    // optional trailer (older firmware does not send it)
    if (const auto n = r.u32())
      i.ble_notifications = *n;
    if (const auto age = r.u32())
      i.ble_last_notify_age_ms = *age;
    if (const auto reports = r.u32())
      i.usb_hid_reports = *reports;
    if (const auto state = r.u8())
      i.link_state = *state;
    if (const auto detail = r.str())
      i.link_detail = *detail;
    return i;
  }
};

// ---- Bonds (paired controllers) ---------------------------------------------------

/// One paired (bonded) controller.
struct BondInfo {
  std::array<uint8_t, 6>
      address{};           ///< BLE identity address, as stored (little-endian, NimBLE order)
  uint8_t address_type{0}; ///< BLE address type (0 public, 1 random)
  bool connected{false};   ///< this controller is the one currently connected
  std::string label;       ///< the controller's name as it reported it (GAP Device Name or
                           ///< advertised name) when it last connected; "" = unknown

  bool operator==(const BondInfo &) const = default;

  /// Human-readable MAC ("AA:BB:CC:DD:EE:FF", most-significant byte first).
  std::string address_string() const {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string s;
    for (int i = 5; i >= 0; --i) {
      s.push_back(hex[address[i] >> 4]);
      s.push_back(hex[address[i] & 0x0F]);
      if (i)
        s.push_back(':');
    }
    return s;
  }
};

static constexpr size_t kBondAddressSize = 6;

/// BONDS payload: [version u8][count u8] then count x [addr 6B][type u8][connected u8][label str]
inline std::vector<uint8_t> serialize_bonds(const std::vector<BondInfo> &bonds) {
  std::vector<uint8_t> out;
  put_u8(out, kProtocolVersion);
  put_u8(out, static_cast<uint8_t>(std::min<size_t>(bonds.size(), 255)));
  size_t n = 0;
  for (const auto &b : bonds) {
    if (n++ == 255)
      break;
    out.insert(out.end(), b.address.begin(), b.address.end());
    put_u8(out, b.address_type);
    put_u8(out, b.connected ? 1 : 0);
    put_str(out, b.label);
  }
  return out;
}

inline std::optional<std::vector<BondInfo>> parse_bonds(std::span<const uint8_t> payload) {
  Reader r(payload);
  const auto version = r.u8();
  const auto count = r.u8();
  if (!version || !count || *version != kProtocolVersion)
    return std::nullopt;
  std::vector<BondInfo> bonds;
  for (uint8_t i = 0; i < *count; ++i) {
    BondInfo b;
    const auto addr = r.bytes(kBondAddressSize);
    const auto type = r.u8();
    const auto connected = r.u8();
    auto label = r.str();
    if (!addr || !type || !connected || !label)
      return std::nullopt;
    std::copy(addr->begin(), addr->end(), b.address.begin());
    b.address_type = *type;
    b.connected = *connected != 0;
    b.label = std::move(*label);
    bonds.push_back(std::move(b));
  }
  return bonds;
}

/// FORGET_BOND payload: [addr 6B][type u8]
inline std::vector<uint8_t> make_forget_bond_payload(const std::array<uint8_t, 6> &address,
                                                     uint8_t address_type) {
  std::vector<uint8_t> out(address.begin(), address.end());
  put_u8(out, address_type);
  return out;
}

inline std::optional<std::pair<std::array<uint8_t, 6>, uint8_t>>
parse_forget_bond_payload(std::span<const uint8_t> payload) {
  if (payload.size() != kBondAddressSize + 1)
    return std::nullopt;
  std::array<uint8_t, 6> address{};
  std::copy(payload.begin(), payload.begin() + kBondAddressSize, address.begin());
  return std::make_pair(address, payload[kBondAddressSize]);
}

// ---- OK / ERROR ------------------------------------------------------------------

inline std::vector<uint8_t> make_ok_payload(Msg request) { return {static_cast<uint8_t>(request)}; }

inline std::vector<uint8_t> make_error_payload(Msg request, ErrorCode code,
                                               std::string_view message) {
  std::vector<uint8_t> out;
  put_u8(out, static_cast<uint8_t>(request));
  put_u32(out, static_cast<uint32_t>(code));
  out.insert(out.end(), message.begin(), message.end());
  return out;
}

struct ErrorPayload {
  Msg request{Msg::GetInfo};
  ErrorCode code{ErrorCode::Failed};
  std::string message;
};

inline std::optional<ErrorPayload> parse_error_payload(std::span<const uint8_t> payload) {
  Reader r(payload);
  const auto request = r.u8();
  const auto code = r.u32();
  if (!request || !code)
    return std::nullopt;
  const auto rest = r.bytes(payload.size() - 5);
  return ErrorPayload{static_cast<Msg>(*request), static_cast<ErrorCode>(*code),
                      std::string(rest->begin(), rest->end())};
}

} // namespace device_config
