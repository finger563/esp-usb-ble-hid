#include "usb.hpp"

#include "esp_timer.h"
#include "tusb.h" // tud_connect / tud_disconnect (pull-up re-arm)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "esp_mac.h"

#include "format.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "usb_device.hpp"

#include "bsp.hpp"

using namespace std::chrono_literals;

static espp::Logger logger({.tag = "USB", .level = espp::Logger::Verbosity::INFO});

static std::unique_ptr<espp::UsbDevice> usb;
static std::shared_ptr<espp::SwitchPro> controller;
static std::atomic<bool> mounted{false};
// The Switch sometimes drops the device without a new bus transaction (e.g.
// after the Change Grip/Order screen): the pull-up is re-armed after the
// link has been down this long, so the host enumerates us again.
static constexpr int64_t kReenumerateAfterUs = 2500 * 1000;
static std::atomic<bool> ever_mounted{false};
static std::atomic<int64_t> unmounted_at_us{0};
static std::atomic<uint32_t> hid_reports_sent{0}; // input reports accepted by TinyUSB

// --- HID TX: handshake replies + the streamed input report ---------------------
//
// Replies are produced on the TinyUSB task (when a host OUTPUT report arrives)
// and on mount; they are queued and sent from one sender task so
// write_hid_report() is never called from the TinyUSB task (where waiting for
// the endpoint to drain would deadlock).
struct OutReport {
  uint8_t id{0};
  std::vector<uint8_t> data;
};
static std::deque<OutReport> hid_tx_queue;
static std::mutex hid_tx_mutex;
static std::condition_variable hid_tx_cv;
static std::unique_ptr<espp::Task> hid_sender_task;

static void enqueue_hid(espp::SwitchPro::ReportData rd) {
  {
    std::lock_guard<std::mutex> lock(hid_tx_mutex);
    // cap the queue so a misbehaving host cannot grow it without bound
    if (hid_tx_queue.size() < 16)
      hid_tx_queue.push_back({rd.first, std::move(rd.second)});
  }
  hid_tx_cv.notify_one();
}

static bool hid_sender_fn(std::mutex &, std::condition_variable &) {
  OutReport rep;
  bool have = false;
  {
    std::unique_lock<std::mutex> lock(hid_tx_mutex);
    // the real Pro Controller reports every 8 ms (full-speed HID poll rate)
    hid_tx_cv.wait_for(lock, 8ms, [] { return !hid_tx_queue.empty(); });
    if (!hid_tx_queue.empty()) {
      rep = std::move(hid_tx_queue.front());
      hid_tx_queue.pop_front();
      have = true;
    }
  }
  if (!mounted.load()) {
    // Re-arm the pull-up if a host that had us configured let the link drop
    // and has not re-enumerated on its own (never before the first mount, so
    // a slow initial enumeration is not interrupted). Runs on our own task.
    if (ever_mounted.load() &&
        esp_timer_get_time() - unmounted_at_us.load() >= kReenumerateAfterUs) {
      logger.info("USB link down for {} ms without re-enumeration; re-arming the pull-up",
                  kReenumerateAfterUs / 1000);
      unmounted_at_us.store(esp_timer_get_time());
      tud_disconnect();
      std::this_thread::sleep_for(20ms);
      tud_connect();
    }
    return false;
  }
  std::error_code ec;
  if (have) {
    // a handshake reply: this is our own task, so retry until the endpoint frees
    for (int i = 0; i < 20 && !usb->write_hid_report(rep.id, rep.data, ec); ++i)
      std::this_thread::sleep_for(1ms);
  } else if (controller->is_ready()) {
    const auto report = controller->get_input_report();
    if (!report.empty() &&
        usb->write_hid_report(controller->input_report_id(), report, ec)) // best-effort
      hid_reports_sent.fetch_add(1);
  }
  return false; // don't stop the task
}

#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
// --- Vendor RX: espp::DispatcherWorker ------------------------------------------
//
// Bytes arrive on the TinyUSB task, where handlers must not block (an OTA
// BEGIN erases a partition, a core-dump erase takes tens of milliseconds). The
// worker queues them (bounded) and runs the module handlers -- and their
// replies -- on its own task; on overflow it resynchronizes the parser and
// tells the services so an in-flight transfer can be aborted.
static std::unique_ptr<espp::DispatcherWorker> vendor_link;
// set from the app task, invoked from the worker: guard it so it may be
// (re)set at any time without racing the worker
static std::mutex rx_overflow_callback_mutex;
static std::function<void()> rx_overflow_callback;

static void on_vendor_receive(std::span<const uint8_t> data) {
  // TinyUSB task context: just queue the bytes and wake the worker.
  usb_dispatcher().push(data);
}

espp::DispatcherWorker &usb_dispatcher() {
  if (!vendor_link) {
    vendor_link = std::make_unique<espp::DispatcherWorker>(espp::DispatcherWorker::Config{
        .send = [](std::span<const uint8_t> frame) { usb_write_vendor(frame); },
        .on_overflow =
            [] {
              std::function<void()> callback;
              {
                std::lock_guard<std::mutex> lock(rx_overflow_callback_mutex);
                callback = rx_overflow_callback;
              }
              if (callback)
                callback(); // on the worker: it may send frames / block
            },
        .task_config = {.name = "usb_rx", .stack_size_bytes = 8192},
        .log_level = espp::Logger::Verbosity::INFO});
  }
  return *vendor_link;
}

void usb_set_rx_overflow_callback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(rx_overflow_callback_mutex);
  rx_overflow_callback = std::move(callback);
}
#endif // CONFIG_DONGLE_USB_VENDOR_INTERFACE

// --- public API ------------------------------------------------------------------

bool usb_is_mounted() { return mounted.load(); }

bool usb_hid_ready() { return mounted.load() && controller && controller->is_ready(); }

uint32_t usb_hid_reports_sent() { return hid_reports_sent.load(); }

bool usb_write_vendor(std::span<const uint8_t> frame) {
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  if (!usb || !mounted.load())
    return false;
  std::error_code ec;
  return usb->write_vendor(frame, ec);
#else
  (void)frame;
  return false;
#endif
}

bool start_usb(const std::shared_ptr<espp::SwitchPro> &ctrl) {
  controller = ctrl;

  // The USB identity must be Nintendo's Pro Controller for a Switch to bind it.
  espp::UsbDevice::Config cfg;
  cfg.vid = espp::SwitchPro::vid;
  cfg.pid = espp::SwitchPro::pid;
  cfg.manufacturer = espp::SwitchPro::manufacturer_name;
  cfg.product = espp::SwitchPro::product_name;
  // a stable, per-device serial (derived from the base MAC) so hosts and the
  // browser console can tell dongles apart
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  cfg.serial_number = fmt::format("{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}", mac[0], mac[1], mac[2],
                                  mac[3], mac[4], mac[5]);
  cfg.log_level = espp::Logger::Verbosity::WARN;

  espp::UsbDevice::HidFunction hid;
  hid.interface_name = "Switch Pro Controller";
  hid.report_descriptor = controller->get_report_descriptor();
  hid.has_out_endpoint = true; // receive the host's OUTPUT reports (the handshake)
  hid.poll_interval_ms = 8;    // the real Pro Controller polls at 8 ms
  hid.on_receive = [](std::span<const uint8_t> data) {
    // TinyUSB task context: compute the reply and queue it (don't send here)
    if (data.empty())
      return;
    if (auto reply = controller->on_hid_report(data[0], data.data(), data.size()))
      enqueue_hid(std::move(*reply));
  };
  cfg.hid = hid;

#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  espp::UsbDevice::VendorFunction vendor;
  vendor.interface_name = "Dongle Console";
  vendor.webusb = true; // BOS / WebUSB / MS OS 2.0 descriptors (driverless on Windows)
  vendor.landing_page_url = "finger563.github.io/esp-usb-ble-hid/dongle_console.html";
  vendor.on_receive = on_vendor_receive;
  cfg.vendor = vendor;
#endif

#if CONFIG_DONGLE_USB_CDC_CONSOLE
  espp::UsbDevice::CdcFunction cdc;
  cdc.interface_name = "Dongle Log Console";
  cdc.route_console = true; // logs over the same cable (teed to UART0)
  cfg.cdc = cdc;
#endif

  usb = std::make_unique<espp::UsbDevice>(cfg);
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  usb_dispatcher(); // exists before the first vendor byte can arrive
#endif

  usb->set_mount_callback([]() {
    logger.info("USB mounted");
    mounted.store(true);
    ever_mounted.store(true);
    // kick off the handshake: the controller proactively sends its device-init
    // (0x81) report
    if (auto init = controller->on_attach())
      enqueue_hid(std::move(*init));
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
    // a frame half-parsed before the (re)connect belongs to the old link
    usb_dispatcher().request_reset();
#endif
  });
  usb->set_unmount_callback([]() {
    logger.info("USB unmounted");
    // timestamp first: the sender reads it right after it sees mounted == false
    unmounted_at_us.store(esp_timer_get_time());
    mounted.store(false);
    {
      std::lock_guard<std::mutex> lock(hid_tx_mutex);
      hid_tx_queue.clear();
    }
#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
    usb_dispatcher().request_reset();
#endif
  });

  std::error_code ec;
  if (!usb->initialize(ec)) {
    logger.error("Failed to initialize USB device: {}", ec.message());
    usb.reset();
    return false;
  }

  hid_sender_task = espp::Task::make_unique(
      {.callback = hid_sender_fn,
       .task_config = {.name = "usb_hid_tx", .stack_size_bytes = 4096, .priority = 10}});
  hid_sender_task->start();

  logger.info("USB initialization DONE (serial {})", cfg.serial_number);
  return true;
}
