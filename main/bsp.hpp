#pragma once

#if CONFIG_TARGET_HARDWARE_QTPY_ESP32_S3
#define HAS_DISPLAY 0
#include "qtpy.hpp"
using Bsp = espp::QtPy;
#elif CONFIG_TARGET_HARDWARE_T3_DONGLE
#define HAS_DISPLAY 1
#include "t-dongle-s3.hpp"
using Bsp = espp::TDongleS3;
#else
#error "No hardware target specified"
#endif

#if HAS_DISPLAY
#include "gui.hpp"
#endif
