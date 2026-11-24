#pragma once

// Switch Pro host rumble decode. Pure (standard library only) so it can be
// host-tested: main/test/switch_rumble_host_test.cpp.

#include <algorithm>
#include <cstdint>
#include <span>

// Switch Pro rumble data: 4 bytes per motor, left first (dekuNukem's
// Nintendo_Switch_Reverse_Engineering, "Rumble data"). Per motor:
//   [0] high-band frequency, low byte
//   [1] bit0: high-band frequency high bit; bits 1-7: high-band amplitude
//       (0..100, twice the 0..1 value)
//   [2] low-band frequency (+0x80 selects the intermediate amplitude range)
//   [3] low-band amplitude, 0x40..0x72 for 0..1
// The dongle has two motors to drive, so take the stronger of the two bands
// as the motor level.
inline uint8_t switch_rumble_level(const uint8_t *motor) {
  const float hf = std::min<int>(motor[1] >> 1, 100) / 100.0f;
  const float lf =
      motor[3] > 0x40 ? std::min<int>(motor[3] - 0x40, 0x32) / static_cast<float>(0x32) : 0.0f;
  return static_cast<uint8_t>(std::max(hf, lf) * 255.0f);
}

// The rumble part of a host OUTPUT report: 0x10 (rumble-only) or 0x01 (rumble
// + subcommand). Both carry a packet number after the report id, so the 8
// rumble bytes sit at bytes 2..9 in both (espp's switch_pro subcommand parser
// agrees: the 0x01 subcommand starts at byte 10). False for other reports or
// a truncated one.
inline bool switch_rumble_decode(std::span<const uint8_t> data, uint8_t &left, uint8_t &right) {
  if (data.size() < 10 || (data[0] != 0x10 && data[0] != 0x01))
    return false;
  left = switch_rumble_level(data.data() + 2);
  right = switch_rumble_level(data.data() + 6);
  return true;
}