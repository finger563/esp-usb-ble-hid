#pragma once

// Board support for a generic ESP32-S3 dev board (e.g. an ESP32-S3-DevKitC
// clone): the BOOT button (GPIO0, active low) is the pairing button and an
// optional WS2812 on CONFIG_GENERIC_S3_LED_GPIO is the status LED (-1 = none).
// It exposes the same small surface the app uses from the other boards:
// initialize_led(), led(), initialize_button().

#include <atomic>
#include <memory>

#include "sdkconfig.h"

#include "base_component.hpp"
#include "color.hpp"
#include "interrupt.hpp"
#include "neopixel.hpp"

class GenericS3 : public espp::BaseComponent {
public:
  using button_callback_t = espp::Interrupt::event_callback_fn;

  static GenericS3 &get() {
    static GenericS3 instance;
    return instance;
  }

  GenericS3(const GenericS3 &) = delete;
  GenericS3 &operator=(const GenericS3 &) = delete;

  bool initialize_button(const button_callback_t &callback = nullptr) {
    button_callback_ = callback;
    if (!button_initialized_.exchange(true))
      interrupts_.add_interrupt(button_pin_);
    return true;
  }

  bool initialize_led() {
    if (led_)
      return false;
    if (CONFIG_GENERIC_S3_LED_GPIO < 0)
      return true; // no LED on this board: led() calls are no-ops
    led_ = std::make_shared<espp::Neopixel>(
        espp::Neopixel::Config{.data_gpio = CONFIG_GENERIC_S3_LED_GPIO});
    return true;
  }

  bool led(const espp::Hsv &hsv) { return led(hsv.rgb()); }

  bool led(const espp::Rgb &rgb) {
    if (!led_)
      return false;
    led_->set_color(rgb);
    led_->show();
    return true;
  }

protected:
  GenericS3()
      : BaseComponent("Generic ESP32-S3") {}

  espp::Interrupt::PinConfig button_pin_{
      .gpio_num = GPIO_NUM_0, // BOOT button, active low
      .callback =
          [this](const auto &event) {
            if (button_callback_)
              button_callback_(event);
          },
      .active_level = espp::Interrupt::ActiveLevel::LOW,
      .interrupt_type = espp::Interrupt::Type::ANY_EDGE,
      .pullup_enabled = true};

  // the button pin is only added once initialize_button() is called
  espp::Interrupt interrupts_{{.interrupts = {},
                               .task_config = {.name = "button interrupts",
                                               .stack_size_bytes = 4096,
                                               .priority = 5,
                                               .core_id = -1}}};

  std::atomic<bool> button_initialized_{false};
  button_callback_t button_callback_{nullptr};
  std::shared_ptr<espp::Neopixel> led_;
};
