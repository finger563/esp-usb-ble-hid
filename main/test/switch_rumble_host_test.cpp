// Host-side unit test for the Switch Pro rumble decode. The header is
// standard-library only, so this builds on a host:
/*
  c++ -std=c++20 -Wall -Wextra -Werror -I main \
      main/test/switch_rumble_host_test.cpp -lm -o sr_test && ./sr_test
*/

#include <cstdint>
#include <cstdio>
#include <vector>

#include "switch_rumble.hpp"

static int g_failures = 0;
#define CHECK(cond)                                                                                \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                                      \
      ++g_failures;                                                                                \
    }                                                                                              \
  } while (0)

int main() {
  // ---- per-motor level: the stronger of the two bands ----
  {
    const uint8_t hf_full[4] = {0x00, 0xC8, 0x00, 0x00}; // amplitude 100 -> 1.0
    const uint8_t hf_half[4] = {0x00, 0x64, 0x00, 0x00}; // amplitude 50 -> 0.5
    const uint8_t lf_full[4] = {0x00, 0x00, 0x80, 0x72}; // 0x72 - 0x40 = 0x32 -> 1.0
    const uint8_t lf_half[4] = {0x00, 0x00, 0x80, 0x59}; // 25 / 50 -> 0.5
    const uint8_t neutral[4] = {0x00, 0x00, 0x00, 0x00};
    const uint8_t hf_over[4] = {0x00, 0xFF, 0x00, 0x00}; // amplitude clamped to 100
    const uint8_t lf_over[4] = {0x00, 0x00, 0x80, 0xFF}; // amplitude clamped to 0x32
    CHECK(switch_rumble_level(hf_full) == 255);
    CHECK(switch_rumble_level(hf_half) == 127); // 0.5 * 255 truncates
    CHECK(switch_rumble_level(lf_full) == 255);
    CHECK(switch_rumble_level(lf_half) == 127);
    CHECK(switch_rumble_level(neutral) == 0);
    CHECK(switch_rumble_level(hf_over) == 255);
    CHECK(switch_rumble_level(lf_over) == 255);
  }

  // ---- 0x10 (rumble-only): [0x10][packet number][8 rumble bytes] ----
  // The packet number cycles 0x0..0xF and must NOT be decoded as rumble data:
  // with a non-zero packet number and non-zero motors, reading one byte early
  // shifts every motor byte and produces different levels.
  {
    const std::vector<uint8_t> packet = {0x10, 0x05, 0x00, 0xC8, 0x00, 0x00, // left: full
                                         0x00, 0x64, 0x00, 0x00};           // right: half
    uint8_t left = 0, right = 0;
    CHECK(switch_rumble_decode(packet, left, right));
    CHECK(left == 255);
    CHECK(right == 127);
  }

  // ---- 0x01 (rumble + subcommand): same rumble offset, subcommand ignored ----
  {
    const std::vector<uint8_t> packet = {0x01, 0x0A, 0x00, 0xC8, 0x00, 0x00, // left: full
                                         0x00, 0x64, 0x00, 0x00,            // right: half
                                         0x01, 0x00, 0x00, 0x00};           // subcommand tail
    uint8_t left = 0, right = 0;
    CHECK(switch_rumble_decode(packet, left, right));
    CHECK(left == 255);
    CHECK(right == 127);
  }

  // ---- low-band motors, neutral rumble ----
  {
    const std::vector<uint8_t> packet = {0x10, 0x0F, 0x00, 0x00, 0x80, 0x72, // left: full
                                         0x00, 0x00, 0x80, 0x59};           // right: half
    uint8_t left = 0, right = 0;
    CHECK(switch_rumble_decode(packet, left, right));
    CHECK(left == 255);
    CHECK(right == 127);

    const std::vector<uint8_t> neutral = {0x10, 0x03, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(switch_rumble_decode(neutral, left, right));
    CHECK(left == 0 && right == 0);
  }

  // ---- reports without rumble data ----
  {
    uint8_t left = 0, right = 0;
    const std::vector<uint8_t> truncated = {0x10, 0x05, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x64, 0x00};
    CHECK(!switch_rumble_decode(truncated, left, right)); // 9 bytes: one short
    std::vector<uint8_t> input(12, 0x00);
    input[0] = 0x81; // an INPUT report: no rumble
    CHECK(!switch_rumble_decode(input, left, right));
    CHECK(!switch_rumble_decode({}, left, right));
  }

  if (g_failures) {
    std::printf("%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}