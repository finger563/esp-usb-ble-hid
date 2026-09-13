#pragma once

// Status LED helpers: every LED write goes through here so the user-configured
// brightness (device settings) applies to the whole app.

#include <algorithm>
#include <atomic>

#include "color.hpp"

#include "bsp.hpp"

/// LED brightness scale, 0..1 (set from the persisted settings).
inline std::atomic<float> g_led_brightness{1.0f};

inline void set_led_brightness_percent(uint8_t percent) {
  g_led_brightness.store(std::clamp(percent, uint8_t{0}, uint8_t{100}) / 100.0f);
}

inline void set_led(const espp::Hsv &hsv) {
  espp::Hsv scaled = hsv;
  scaled.v *= g_led_brightness.load();
  Bsp::get().led(scaled);
}

inline void set_led(const espp::Rgb &rgb) { set_led(rgb.hsv()); }
