SET(SOURCES screens/ui_MainScreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_xbox_png.c
    images/ui_img_bt_png.c
    images/ui_img_922611512.c
    images/ui_img_switch_png.c
    images/ui_img_usb_png.c)

add_library(ui ${SOURCES})
