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

// Each side of the screen is a big icon (the controller on the BLE side, the
// host on the USB side) with a small link badge under it. The badge appears
// when the link is up, and the big icon shows its own colors then; while the
// link is down it is a white silhouette (the design's recolor).
static void show_link(lv_obj_t *icon, lv_obj_t *badge, bool connected) {
  if (connected) {
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_image_recolor(icon, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(icon, connected ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
}

void Gui::set_usb_connected(bool connected) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  show_link(ui_SwitchIcon, ui_UsbIcon, connected);
}

void Gui::set_usb_host(UsbHost host) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // the USB side of the screen shows what the dongle is to the host
  // ui_SwitchIcon (the image object keeps its name from the design); the
  // link state's recolor (set_usb_connected) applies to either image
  lv_image_set_src(ui_SwitchIcon, host == UsbHost::Xbox360 ? &ui_img_xbox_png : &ui_img_switch_png);
}

void Gui::set_ble_connected(bool connected) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  show_link(ui_XboxIcon, ui_BtIcon, connected);
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
