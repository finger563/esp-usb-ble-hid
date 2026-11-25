#include "rumble.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "esp_timer.h"

#include "ble.hpp"
#include "format.hpp"
#include "gamepad_state_hid_rp.hpp" // espp::to_rumble_levels
#include "logger.hpp"
#include "task.hpp"

namespace {

espp::Logger logger({.tag = "rumble", .level = espp::Logger::Verbosity::INFO});

// Everything below is guarded by `mutex`; the BLE write happens without it.
std::mutex mutex;
std::condition_variable cv;
bool changed = false;                               // wakes the sender
espp::RumbleState host_target;                      // what the USB host asks for (raw)
espp::RumbleOptions controller_options;             // the connected controller's (applied on send)
std::optional<espp::hid_rp::RumbleEncoder> encoder; // the connected controller's
uint32_t generation = 0;                            // bumped when the controller changes
espp::RumbleState last_sent;                        // what the controller was last told (shaped)
int64_t last_send_us = 0;                           // when (0: never, on this controller)
uint32_t stop_repeats_left = 0;                     // extra stop reports still to send
int64_t test_start_us = 0;                          // a test pattern overrides the host
int64_t test_until_us = 0;                          // from test_start_us until test_until_us
std::optional<espp::RumbleOptions> test_preview;    // the test plays through these, if set
std::unique_ptr<espp::Task> task;

constexpr int64_t kIdleWaitUs = 100'000; // also notices the output report appearing
constexpr int64_t kStopGapUs = 50'000;   // between the repeated stop reports
constexpr int64_t kTestUs = int64_t{RUMBLE_TEST_MS} * 1000;
constexpr uint8_t kTestLevel = 128; // half strength, so a strength above 100 % is audible too

std::string describe(const espp::RumbleOptions &o) {
  if (o.is_off())
    return "off";
  return fmt::format("strength {}%{}", o.strength_percent,
                     o.swaps_motors() ? ", motors swapped" : "");
}

bool test_active(int64_t now) { return now < test_until_us; }
int64_t test_half_us() { return test_start_us + kTestUs / 2; }

// When the test pattern changes next (it is running): half way, then its end.
int64_t test_next_change(int64_t now) {
  return now < test_half_us() ? test_half_us() : test_until_us;
}

// The test pattern (while it runs): the strong motor alone for the first half,
// then the weak motor alone, so a motor swap can be heard.
espp::RumbleState test_levels(int64_t now) {
  using Motor = espp::RumbleState::Motor;
  espp::RumbleState t;
  t.set(now < test_half_us() ? Motor::Strong : Motor::Weak, kTestLevel);
  return t;
}

// What the controller should play now: the test pattern (through its preview
// options, if any) or the host's request, shaped by the rumble options. Always
// computed from the raw request, so the options apply exactly once.
espp::RumbleState effective_target(int64_t now) {
  if (test_active(now))
    return test_preview.value_or(controller_options).apply(test_levels(now));
  return controller_options.apply(host_target);
}

bool can_rumble_locked() { return encoder && ble_has_output_report(encoder->report_id()); }

// One step: send what is due, or wait until something may be. Dedupe, keep-
// alive and stop repeats all work on the shaped levels: an all-zero result is
// a stop, so options that turn the motors off stop them at once.
bool sender_step(std::mutex &, std::condition_variable &) {
  std::unique_lock<std::mutex> lk(mutex);
  const int64_t now = esp_timer_get_time();
  const espp::RumbleState want = effective_target(now);
  int64_t wait_us = kIdleWaitUs;
  std::optional<espp::RumbleState> send;
  if (can_rumble_locked()) {
    const int64_t since = last_send_us ? now - last_send_us : INT64_MAX;
    const int64_t min_gap = int64_t{RUMBLE_MIN_INTERVAL_MS} * 1000;
    const int64_t keepalive = int64_t{RUMBLE_KEEPALIVE_MS} * 1000;
    if (want != last_sent) {
      if (since >= min_gap)
        send = want;
      else
        wait_us = min_gap - since;
    } else if (want.any()) {
      if (since >= keepalive)
        send = want; // re-arm the pulse while the motors should run
      else
        wait_us = keepalive - since;
    } else if (stop_repeats_left > 0) {
      if (since >= kStopGapUs)
        send = want;
      else
        wait_us = kStopGapUs - since;
    }
    if (test_active(now))
      wait_us = std::min(wait_us, test_next_change(now) - now); // its next half, or its end
  }
  if (!send) {
    changed = false;
    cv.wait_for(lk, std::chrono::microseconds(wait_us), [] { return changed; });
    return false; // keep the task running
  }

  const uint32_t gen = generation;
  const uint8_t report_id = encoder->report_id();
  const auto payload =
      encoder->encode(espp::to_rumble_levels(*send), send->any() ? RUMBLE_PULSE_MS : 0);
  const bool repeat_stop = !send->any() && *send == last_sent;
  lk.unlock();
  const bool ok = ble_write_output_report(report_id, payload);
  lk.lock();
  if (gen != generation)
    return false; // the controller changed while writing: its state starts over
  last_send_us = esp_timer_get_time(); // a failed write is retried after the same gap
  if (!ok)
    return false;
  if (repeat_stop) {
    stop_repeats_left -= stop_repeats_left ? 1 : 0;
  } else {
    if (!send->any())
      stop_repeats_left = RUMBLE_STOP_REPEATS; // a stop: send it a few more times
    else
      stop_repeats_left = 0;
    if (send->any() != last_sent.any())
      logger.debug("rumble {}", send->any() ? "on" : "off");
    last_sent = *send;
  }
  return false;
}

} // namespace

void rumble_start() {
  if (task)
    return;
  task = espp::Task::make_unique(
      {.callback = sender_step, .task_config = {.name = "rumble", .stack_size_bytes = 4096}});
  task->start();
}

void rumble_set_target(const espp::RumbleState &target) {
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (target == host_target)
      return; // unchanged (a Switch host repeats its rumble in every report)
    host_target = target;
    changed = true;
  }
  cv.notify_one();
}

void rumble_set_options(const espp::RumbleOptions &options) {
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (options == controller_options)
      return; // unchanged (every connect sets them)
    controller_options = options;
    changed = true; // re-shape what is playing now
  }
  cv.notify_one();
  logger.info("Rumble options: {}", describe(options));
}

void rumble_set_encoder(std::optional<espp::hid_rp::RumbleEncoder> new_encoder) {
  {
    std::lock_guard<std::mutex> lk(mutex);
    encoder = std::move(new_encoder);
    ++generation;
    last_sent = {}; // a (re)connected controller starts with its motors off
    last_send_us = 0;
    stop_repeats_left = 0;
    test_start_us = 0;
    test_until_us = 0;
    test_preview.reset();
    changed = true;
  }
  cv.notify_one();
}

bool rumble_supported() {
  std::lock_guard<std::mutex> lk(mutex);
  return can_rumble_locked();
}

RumbleTestResult rumble_test(const std::optional<espp::RumbleOptions> &preview) {
  if (!is_ble_subscribed())
    return RumbleTestResult::NoController;
  espp::RumbleOptions played;
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (!can_rumble_locked())
      return RumbleTestResult::CannotRumble;
    played = preview.value_or(controller_options);
    if (played.is_off() || played.strength_percent == 0)
      return RumbleTestResult::Off; // the pattern would be all stops
    test_start_us = esp_timer_get_time();
    test_until_us = test_start_us + kTestUs;
    test_preview = preview;
    changed = true;
  }
  cv.notify_one();
  logger.info("Rumble test ({} options: {})", preview ? "preview" : "saved", describe(played));
  return RumbleTestResult::Ok;
}
