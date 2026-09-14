#include "usb.hpp"

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
  if (!mounted.load())
    return false;
  std::error_code ec;
  if (have) {
    // a handshake reply: this is our own task, so retry until the endpoint frees
    for (int i = 0; i < 20 && !usb->write_hid_report(rep.id, rep.data, ec); ++i)
      std::this_thread::sleep_for(1ms);
  } else if (controller->is_ready()) {
    const auto report = controller->get_input_report();
    if (!report.empty())
      usb->write_hid_report(controller->input_report_id(), report, ec); // best-effort
  }
  return false; // don't stop the task
}

// --- Vendor RX: queue on the TinyUSB task, dispatch from a worker ---------------
//
// Handlers can block (OTA flash writes, core-dump erase), so bytes are queued and
// fed to the dispatcher from a worker task. The protocols are one-request-in-
// flight, so a well-behaved host queues at most ~one frame; the cap protects
// against a misbehaving host.
static espp::Dispatcher dispatcher;
// set from the app task, invoked from the RX worker: guard it so it may be
// (re)set at any time without racing the worker
static std::mutex rx_overflow_callback_mutex;
static std::function<void()> rx_overflow_callback;
static std::mutex rx_mutex;
static std::condition_variable rx_cv;
static std::deque<std::vector<uint8_t>> rx_queue;
static size_t rx_queued_bytes = 0;
static bool rx_overflow = false;
static constexpr size_t kMaxQueuedRxBytes = 8 * espp::stream_frame::kMaxFrameSize;
static std::unique_ptr<espp::Task> rx_task;

static void on_vendor_receive(std::span<const uint8_t> data) {
  // TinyUSB task context: just queue the bytes and wake the worker.
  {
    std::lock_guard<std::mutex> lock(rx_mutex);
    if (rx_queued_bytes + data.size() > kMaxQueuedRxBytes) {
      // partial frames are useless once bytes are missing: drop everything and
      // let the worker resynchronize
      rx_queue.clear();
      rx_queued_bytes = 0;
      rx_overflow = true;
    } else {
      rx_queue.emplace_back(data.begin(), data.end());
      rx_queued_bytes += data.size();
    }
  }
  rx_cv.notify_one();
}

static bool rx_worker_fn(std::mutex &, std::condition_variable &) {
  std::deque<std::vector<uint8_t>> chunks;
  bool overflowed = false;
  {
    std::unique_lock<std::mutex> lock(rx_mutex);
    rx_cv.wait_for(lock, 100ms, [] { return !rx_queue.empty() || rx_overflow; });
    std::swap(chunks, rx_queue);
    rx_queued_bytes = 0;
    overflowed = rx_overflow;
    rx_overflow = false;
  }
  if (overflowed) {
    logger.warn("vendor RX overflow: frames dropped");
    dispatcher.reset();
    std::function<void()> callback;
    {
      std::lock_guard<std::mutex> lock(rx_overflow_callback_mutex);
      callback = rx_overflow_callback;
    }
    if (callback)
      callback(); // invoked outside the lock: it may send frames / block
    return false; // the dropped chunks are gone; nothing to parse
  }
  for (const auto &chunk : chunks)
    dispatcher.feed(chunk);
  return false; // don't stop the task
}

// --- public API ------------------------------------------------------------------

espp::Dispatcher &usb_dispatcher() { return dispatcher; }

void usb_set_rx_overflow_callback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(rx_overflow_callback_mutex);
  rx_overflow_callback = std::move(callback);
}

bool usb_is_mounted() { return mounted.load(); }

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

  usb->set_mount_callback([]() {
    logger.info("USB mounted");
    mounted.store(true);
    // kick off the handshake: the controller proactively sends its device-init
    // (0x81) report
    if (auto init = controller->on_attach())
      enqueue_hid(std::move(*init));
  });
  usb->set_unmount_callback([]() {
    logger.info("USB unmounted");
    mounted.store(false);
    std::lock_guard<std::mutex> lock(hid_tx_mutex);
    hid_tx_queue.clear();
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

#if CONFIG_DONGLE_USB_VENDOR_INTERFACE
  rx_task = espp::Task::make_unique(
      {.callback = rx_worker_fn, .task_config = {.name = "usb_rx", .stack_size_bytes = 8192}});
  rx_task->start();
#endif

  logger.info("USB initialization DONE (serial {})", cfg.serial_number);
  return true;
}
