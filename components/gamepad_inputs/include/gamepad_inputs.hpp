#pragma once

#include <string>
#include <vector>

struct GamepadInputs {
  struct Buttons {
    union {
      uint32_t raw{0};
      // NOTE: This is the most common shared layout
      struct {
        // byte 0
        uint8_t a : 1;
        uint8_t b : 1;
        uint8_t x : 1;
        uint8_t y : 1;
        uint8_t l1 : 1;
        uint8_t r1 : 1;
        uint8_t l2 : 1;
        uint8_t r2 : 1;
        // byte 1
        uint8_t l3 : 1;
        uint8_t r3 : 1;
        uint8_t up : 1;
        uint8_t down : 1;
        uint8_t left : 1;
        uint8_t right : 1;
        uint8_t home : 1;
        uint8_t capture : 1;
        // byte 2
        uint8_t start : 1;
        uint8_t select : 1;
      } __attribute__((packed));
      // NOTE: this is for the Xbox
      struct {
        // byte 0, 1
        uint16_t : 16;
        // byte 2
        uint8_t menu : 1;    // overlapped with start
        uint8_t options : 1; // overlapped with select
      } __attribute__((packed));
      // NOTE: this is for the Switch Pro Controller
      struct {
        // byte 0
        uint8_t : 4;
        uint8_t l : 1;  // overlapped with l1
        uint8_t r : 1;  // overlapped with r1
        uint8_t zl : 1; // overlapped with l2
        uint8_t zr : 1; // overlapped with r2
        // byte 1
        uint8_t thumb_l : 1;    // overlapped with l3
        uint8_t thumb_r : 1;    // overlapped with r3
        uint8_t dpad_up : 1;    // overlapped with up
        uint8_t dpad_down : 1;  // overlapped with down
        uint8_t dpad_left : 1;  // overlapped with left
        uint8_t dpad_right : 1; // overlapped with right
        uint8_t : 2;
        // byte 2
        uint8_t minus : 1; // overlapped with select
        uint8_t plus : 1;  // overlapped with start
        uint8_t right_sr : 1;
        uint8_t right_sl : 1;
        uint8_t left_sr : 1;
        uint8_t left_sl : 1;
      } __attribute__((packed));
    };
  } __attribute__((packed));

  struct Joystick {
    float x{0.0f}; // range [-1, 1]
    float y{0.0f}; // range [-1, 1]
  };

  struct Trigger {
    float value{0.0f}; // range [0, 1]
  };

  Buttons buttons;
  Joystick left_joystick;
  Joystick right_joystick;
  Trigger l2;
  Trigger r2;
};
