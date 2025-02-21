#pragma once

#include <string>
#include <vector>

struct GamepadInputs {
  struct Buttons {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t l1 : 1;
    uint8_t r1 : 1;
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t l3 : 1;
    uint8_t r3 : 1;
    uint8_t start : 1;
    uint8_t select : 1;
    uint8_t up : 1;
    uint8_t down : 1;
    uint8_t left : 1;
    uint8_t right : 1;
    uint8_t home : 1;
    uint8_t capture : 1;
  } __attribute__((packed));

  struct Joystick {
    float x; // range [-1, 1]
    float y; // range [-1, 1]
  };

  struct Trigger {
    float value; // range [0, 1]
  };

  Buttons buttons;
  Joystick left_joystick;
  Joystick right_joystick;
  Trigger l2;
  Trigger r2;
};
