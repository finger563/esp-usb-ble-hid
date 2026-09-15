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

/// The LED colour while a controller is connected (blue, like the scan).
inline const espp::Rgb kLedConnectedColor(0.0f, 0.0f, 1.0f);

/// The steady "controller connected" LED value, after both brightness settings
/// (used to detect when a settings change needs a new write).
inline float led_connected_value() {
  return g_led_connected_level.load() * g_led_brightness.load();
}

/// Show the steady "controller connected" LED.
inline void show_led_connected() {
  espp::Hsv hsv = kLedConnectedColor.hsv();
  hsv.v = g_led_connected_level.load();
  set_led(hsv);
}
