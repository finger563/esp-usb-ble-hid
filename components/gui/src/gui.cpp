#include "gui.hpp"

extern "C" {
#include "ui.h"
#include "ui_helpers.h"
// #include "ui_comp.h" // we don't have any components in this project yet
}

void Gui::deinit_ui() {
  logger_.info("Deinitializing UI");
  // delete the ui
  lv_obj_del(ui_MainScreen);
}

void Gui::init_ui() {
  logger_.info("Initializing UI");
  ui_init();

  set_usb_connected(false);
  set_ble_connected(false);

  // make the label and center it
  label_ = lv_label_create(lv_screen_active());
  lv_label_set_long_mode(label_, LV_LABEL_LONG_WRAP);
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label_, 150);
}

void Gui::set_usb_connected(bool connected) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // hide or show the usb image
  // ui_UsbIcon
  if (connected) {
    lv_obj_clear_flag(ui_UsbIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_UsbIcon, LV_OBJ_FLAG_HIDDEN);
  }
}

void Gui::set_ble_connected(bool connected) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // hide or show the ble image
  // ui_BtIcon
  if (connected) {
    lv_obj_clear_flag(ui_BtIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_BtIcon, LV_OBJ_FLAG_HIDDEN);
  }
}

void Gui::on_value_changed(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("Value changed: {}", fmt::ptr(target));
}

void Gui::on_pressed(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("PRESSED: {}", fmt::ptr(target));
}

void Gui::on_scroll(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("SCROLL: {}", fmt::ptr(target));
}

void Gui::on_key(lv_event_t *e) {
  // print the key
  auto key = lv_indev_get_key(lv_indev_get_act());
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("KEY: {} on {}", key, fmt::ptr(target));
}
