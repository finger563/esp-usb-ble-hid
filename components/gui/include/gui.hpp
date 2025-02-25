#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "base_component.hpp"
#include "display.hpp"
#include "high_resolution_timer.hpp"

class Gui : public espp::BaseComponent {
public:
  struct Config {
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Gui(const Config &config)
      : BaseComponent("Gui", config.log_level) {
    init_ui();
    logger_.debug("Starting task...");
    // now start the gui updater task
    task_.periodic(16 * 1000);
    using namespace std::placeholders;
  }

  ~Gui() {
    task_.stop();
    deinit_ui();
  }

  void pause() {
    paused_ = true;
    task_.stop();
  }

  void resume() {
    task_.periodic(16 * 1000);
    paused_ = false;
  }

  void set_usb_connected(bool connected);
  void set_ble_connected(bool connected);

  void set_label_text(std::string_view text) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    lv_label_set_text(label_, text.data());
  }

protected:
  void init_ui();
  void deinit_ui();

  void update() {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui *>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_SCROLL:
      gui->on_scroll(e);
      break;
    case LV_EVENT_PRESSED:
    case LV_EVENT_CLICKED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      gui->on_key(e);
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);
  void on_key(lv_event_t *e);
  void on_scroll(lv_event_t *e);

  lv_obj_t *label_{nullptr};

  std::atomic<bool> paused_{false};
  espp::HighResolutionTimer task_{{
      .name = "Gui Task",
      .callback = std::bind(&Gui::update, this),
  }};
  std::recursive_mutex mutex_;
};
