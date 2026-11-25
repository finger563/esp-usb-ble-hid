#pragma once

// Rumble: the USB host's rumble, played on the BLE controller's motors.
//
// The host side (rumble_set_target(), called from the TinyUSB task) says what
// the motors should do; this module's sender task turns that into the
// controller's rumble output report and writes it over BLE, with the timing a
// lossy, rate-limited link needs:
//  - latest wins: only the newest target is sent, and an unchanged one is not
//    sent again (a Switch host repeats its rumble in every report);
//  - at most one write every RUMBLE_MIN_INTERVAL_MS, coalescing faster changes;
//  - each report plays a short pulse (RUMBLE_PULSE_MS), re-sent every
//    RUMBLE_KEEPALIVE_MS while the motors should run, so a lost write - the
//    stop included - leaves them running for one pulse at most;
//  - the stop is sent RUMBLE_STOP_REPEATS more times;
//  - the motors stop when the controller changes or goes away, and when the
//    USB host unmounts or suspends (usb.cpp delivers an all-off target).
//
// The controller's rumble options (espp::RumbleOptions: off / strength / swap
// motors, from its button-map profile) are applied here, once, each time the
// sender decides what to send: the host's request is kept as the host sent it,
// and what is deduplicated, kept alive and stopped is the shaped result. So
// turning rumble off, or changing its strength, takes effect at once, even
// while the host holds a rumble on (an all-zero result is a stop to send).
//
// Which report to write, and how, comes from the controller's own report map
// (espp::hid_rp::RumbleEncoder), so any controller that declares a PID-style
// rumble report is driven, and nothing is written to one that does not.

#include <cstdint>
#include <optional>

#include "hid-rp-rumble.hpp"
#include "rumble_options.hpp"
#include "rumble_state.hpp"

static constexpr uint32_t RUMBLE_PULSE_MS = 300;
static constexpr uint32_t RUMBLE_KEEPALIVE_MS = 150;
static constexpr uint32_t RUMBLE_MIN_INTERVAL_MS = 30;
static constexpr uint32_t RUMBLE_STOP_REPEATS = 2;
/// Length of rumble_test()'s pattern: the strong motor alone for the first
/// half, then the weak motor alone, both at half strength (128).
static constexpr uint32_t RUMBLE_TEST_MS = 600;

/// Why rumble_test() did, or did not, start the test pattern.
enum class RumbleTestResult {
  Ok,           ///< the pattern is playing
  NoController, ///< no controller is connected
  CannotRumble, ///< the connected controller declares no rumble report the dongle can write
  Off,          ///< the options it would play through are off (or at 0 %)
};

/// Start the sender task. Call once at startup.
void rumble_start();

/// The rumble the USB host wants now (any task, non-blocking), as the host
/// asked for it: the rumble options are applied by the sender.
void rumble_set_target(const espp::RumbleState &target);

/// The connected controller's rumble options (its profile's, else the default
/// ones). Applied to whatever plays from the next step on, including a rumble
/// already playing. Not changed by rumble_set_encoder().
void rumble_set_options(const espp::RumbleOptions &options);

/// The connected controller's rumble encoder (from its report map), or nullopt
/// when it has none / when it disconnects. Stops whatever was playing.
void rumble_set_encoder(std::optional<espp::hid_rp::RumbleEncoder> new_encoder);

/// Whether the connected controller can be rumbled: its report map declares a
/// rumble report and the controller exposes that output report over BLE.
bool rumble_supported();

/// Play the test pattern (RUMBLE_TEST_MS) on the connected controller,
/// whatever the host is sending, through @p preview when given (for this test
/// only: the controller's options are not changed), else through the
/// controller's options. Returns at once (the sender plays it); anything but
/// Ok means nothing was started.
RumbleTestResult rumble_test(const std::optional<espp::RumbleOptions> &preview = std::nullopt);
