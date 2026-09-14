# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.20)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/"
)

set(
  COMPONENTS
  "main esptool_py driver logger math task hid-rp hid_service esp-nimble-cpp ble_gatt_server espcoredump gui qtpy t-dongle-s3 xbox switch_pro usb_device esp_tinyusb stream_frame dispatcher ota coredump nvs device_config app_update"
  CACHE STRING
  "List of components to include"
  )

project(esp-usb-ble-hid)

set(CMAKE_CXX_STANDARD 20)
