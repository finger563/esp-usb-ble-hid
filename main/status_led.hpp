#pragma once

// Status LED helpers: every LED write goes through here so the user-configured
// brightness (device settings) applies to the whole app.

#include <algorithm>
#include <atomic>

#include "color.hpp"

#include "bsp.hpp"

/// LED brightness scale, 0..1 (set from the persisted settings).
inline std::atomic<float> g_led_brightness{1.0f};
/// Steady LED level while a controller is connected, 0..1 of g_led_brightness.
inline std::atomic<float> g_led_connected_level{0.25f};
/// Toggle the LED on every input report instead of holding it steady.
inline std::atomic<bool> g_led_activity_blink{false};

inline void set_led_brightness_percent(uint8_t percent) {
  g_led_brightness.store(std::clamp(percent, uint8_t{0}, uint8_t{100}) / 100.0f);
}

inline void set_led(const espp::Hsv &hsv) {
  espp::Hsv scaled = hsv;
  scaled.v *= g_led_brightness.load();
  Bsp::get().led(scaled);
}

inline void set_led(const espp::Rgb &rgb) { set_led(rgb.hsv()); }

inline void set_led_connected_brightness_percent(uint8_t percent) {
  g_led_connected_level.store(std::clamp(percent, uint8_t{0}, uint8_t{100}) / 100.0f);
}
inline void set_led_activity_blink(bool enabled) { g_led_activity_blink.store(enabled); }
inline bool led_activity_blink() { return g_led_activity_blink.load(); }
/// Number of per-input blink writes so far. The link supervisor compares it
/// against what it last saw: a blink write that lands after the supervisor's
/// steady write (the setting was turned off in between, on another task) is
/// detected on the next tick and the steady level is written again.
inline std::atomic<uint32_t> g_led_blink_writes{0};

/// The LED colour while a controller is connected (blue, like the scan).
inline const espp::Rgb kLedConnectedColor(0.0f, 0.0f, 1.0f);

/// The steady "controller connected" LED value, after both brightness settings
/// (used to detect when a settings change needs a new write).
inline float led_connected_value() {
  return g_led_connected_level.load() * g_led_brightness.load();
}

/// The per-input blink (debugging aid): toggle the LED between the connected
/// colour and off. Does nothing unless the blink setting is on.
inline void led_blink_toggle() {
  if (!g_led_activity_blink.load())
    return;
  static std::atomic<bool> led_on{false};
  static const espp::Rgb off(0.0f, 0.0f, 0.0f);
  const bool on = !led_on.load();
  led_on.store(on);
  set_led(on ? kLedConnectedColor : off);
  g_led_blink_writes.fetch_add(1);
}

/// Show the steady "controller connected" LED.
inline void show_led_connected() {
  espp::Hsv hsv = kLedConnectedColor.hsv();
  hsv.v = g_led_connected_level.load();
  set_led(hsv);
}
