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
// Which report to write, and how, comes from the controller's own report map
// (espp::hid_rp::RumbleEncoder), so any controller that declares a PID-style
// rumble report is driven, and nothing is written to one that does not.

#include <cstdint>
#include <optional>

#include "hid-rp-rumble.hpp"
#include "rumble_state.hpp"

static constexpr uint32_t RUMBLE_PULSE_MS = 300;
static constexpr uint32_t RUMBLE_KEEPALIVE_MS = 150;
static constexpr uint32_t RUMBLE_MIN_INTERVAL_MS = 30;
static constexpr uint32_t RUMBLE_STOP_REPEATS = 2;
static constexpr uint32_t RUMBLE_TEST_MS = 400; ///< length of rumble_test()'s pulse

/// Start the sender task. Call once at startup.
void rumble_start();

/// The rumble the USB host wants now (any task, non-blocking).
void rumble_set_target(const espp::RumbleState &target);

/// The connected controller's rumble encoder (from its report map), or nullopt
/// when it has none / when it disconnects. Stops whatever was playing.
void rumble_set_encoder(std::optional<espp::hid_rp::RumbleEncoder> encoder);

/// Whether the connected controller can be rumbled: its report map declares a
/// rumble report and the controller exposes that output report over BLE.
bool rumble_supported();

/// Play a short test rumble on the connected controller (both main motors,
/// RUMBLE_TEST_MS), whatever the host is sending. Returns false when the
/// controller cannot be rumbled.
bool rumble_test();
