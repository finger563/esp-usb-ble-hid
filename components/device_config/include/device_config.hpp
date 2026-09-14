#pragma once

// DeviceConfig: the dongle's configuration module for the espp `dispatcher`.
//
// It answers the protocol in detail/device_config_protocol.hpp on module
// device_config::kModule. The module is transport-agnostic: it only ever emits
// frames through Config::send, and it never touches NVS / BLE / USB itself --
// the application supplies the settings store, the status, and the actions via
// callbacks. Register it with:
//
//   dispatcher.register_module(device_config::kModule,
//                              [&](const espp::stream_frame::Frame &f) { config.handle(f); },
//                              DeviceConfig::module_info());
//
// Threading: handle() runs on whatever task feeds the dispatcher. The settings
// are copied out under a mutex, so settings() may be read from any task (e.g.
// the BLE notification path reads them for every report).

#include <array>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "base_component.hpp"
#include "dispatcher.hpp"
#include "stream_frame.hpp"

#include "detail/device_config_protocol.hpp"

class DeviceConfig : public espp::BaseComponent {
public:
  using Settings = device_config::Settings;
  using Info = device_config::Info;
  using BondInfo = device_config::BondInfo;
  using Action = device_config::Action;
  using Msg = device_config::Msg;
  using ErrorCode = device_config::ErrorCode;

  static constexpr uint8_t kModule = device_config::kModule;

  /// Called with a complete frame to transmit.
  using send_fn = std::function<void(std::span<const uint8_t> frame)>;
  /// Called (outside the settings lock) with new settings; persist and apply
  /// them here. Return false (and set @p error) if they could not be
  /// persisted: the module then keeps the previous settings and replies ERROR
  /// instead of acknowledging values that would be lost on reboot.
  using settings_changed_fn = std::function<bool(const Settings &settings, std::string &error)>;
  /// Returns the live device status.
  using info_fn = std::function<Info()>;
  /// Performs an action; on failure return false and set @p error.
  using action_fn = std::function<bool(Action action, std::string &error)>;
  /// Returns the paired (bonded) controllers.
  using bonds_fn = std::function<std::vector<BondInfo>()>;
  /// Forgets one bond; on failure return false and set @p error.
  using forget_bond_fn = std::function<bool(const std::array<uint8_t, 6> &address,
                                            uint8_t address_type, std::string &error)>;

  struct Config {
    send_fn send{nullptr};
    Settings initial{}; ///< settings at startup (loaded by the app)
    settings_changed_fn on_settings_changed{nullptr};
    info_fn info{nullptr};
    action_fn on_action{nullptr};
    bonds_fn bonds{nullptr};
    forget_bond_fn forget_bond{nullptr};
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit DeviceConfig(const Config &config)
      : BaseComponent("DeviceConfig", config.log_level)
      , send_(config.send)
      , on_settings_changed_(config.on_settings_changed)
      , info_(config.info)
      , on_action_(config.on_action)
      , bonds_(config.bonds)
      , forget_bond_(config.forget_bond)
      , settings_(config.initial) {}

  /// How the module advertises itself in dispatcher discovery. (Firmware update
  /// and crash dumps are separate modules with their own discovery entries.)
  static espp::Dispatcher::ModuleInfo module_info() {
    return {.name = "Device Config",
            .app = "dongle_console.html",
            .description = "Dongle status, settings, paired controllers and actions"};
  }

  /// A copy of the current settings (thread-safe).
  Settings settings() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return settings_;
  }

  /// Dispatcher entry point. Reply-flagged frames are ignored.
  void handle(const espp::stream_frame::Frame &frame) {
    if (frame.is_reply())
      return;
    const auto request = static_cast<Msg>(frame.type);
    switch (request) {
    case Msg::GetInfo: {
      Info info = info_ ? info_() : Info{};
      send(Msg::Info, info.serialize());
      break;
    }
    case Msg::GetSettings:
      send(Msg::Settings, settings().serialize());
      break;
    case Msg::SetSettings: {
      Settings current = settings();
      auto updated = Settings::parse(frame.payload, current);
      if (!updated) {
        send_error(request, ErrorCode::Malformed, "malformed settings payload");
        break;
      }
      if (auto why = updated->validate(); !why.empty()) {
        send_error(request, ErrorCode::InvalidValue, why);
        break;
      }
      if (std::string error; !apply(*updated, error)) {
        send_error(request, ErrorCode::Failed, error);
        break;
      }
      send(Msg::Ok, device_config::make_ok_payload(request));
      send(Msg::Settings, updated->serialize());
      break;
    }
    case Msg::ResetSettings: {
      const Settings defaults{};
      if (std::string error; !apply(defaults, error)) {
        send_error(request, ErrorCode::Failed, error);
        break;
      }
      send(Msg::Ok, device_config::make_ok_payload(request));
      send(Msg::Settings, defaults.serialize());
      break;
    }
    case Msg::Action: {
      if (frame.payload.size() != 1) {
        send_error(request, ErrorCode::Malformed, "ACTION payload must be one byte");
        break;
      }
      const auto action = static_cast<Action>(frame.payload[0]);
      if (action != Action::StartPairing && action != Action::ClearBonds &&
          action != Action::Reboot) {
        send_error(request, ErrorCode::UnknownRequest, "unknown action");
        break;
      }
      std::string error;
      if (!on_action_ || !on_action_(action, error)) {
        send_error(request, ErrorCode::Failed, error.empty() ? "action failed" : error);
        break;
      }
      send(Msg::Ok, device_config::make_ok_payload(request));
      break;
    }
    case Msg::GetBonds:
      send_bonds();
      break;
    case Msg::ForgetBond: {
      const auto target = device_config::parse_forget_bond_payload(frame.payload);
      if (!target) {
        send_error(request, ErrorCode::Malformed, "FORGET_BOND payload must be [addr 6B][type u8]");
        break;
      }
      std::string error;
      if (!forget_bond_ || !forget_bond_(target->first, target->second, error)) {
        send_error(request, ErrorCode::NotFound, error.empty() ? "no such bond" : error);
        break;
      }
      send(Msg::Ok, device_config::make_ok_payload(request));
      send_bonds();
      break;
    }
    default:
      send_error(request, ErrorCode::UnknownRequest, "unknown request");
      break;
    }
  }

protected:
  // Persist first (outside the lock: NVS writes may take a while), and only
  // adopt the new values once they are stored, so a failed write never leaves
  // the running settings out of sync with what the next boot will load.
  bool apply(const Settings &s, std::string &error) {
    if (on_settings_changed_ && !on_settings_changed_(s, error)) {
      if (error.empty())
        error = "settings could not be saved";
      return false;
    }
    std::lock_guard<std::mutex> lk(mutex_);
    settings_ = s;
    return true;
  }

  void send(Msg type, std::span<const uint8_t> payload) {
    if (!send_)
      return;
    send_(espp::stream_frame::build_frame(/*reply=*/true, kModule, static_cast<uint8_t>(type),
                                          payload));
  }

  void send_error(Msg request, ErrorCode code, std::string_view message) {
    logger_.warn("{} failed: {}", static_cast<int>(request), message);
    send(Msg::Error, device_config::make_error_payload(request, code, message));
  }

  void send_bonds() {
    const std::vector<BondInfo> bonds = bonds_ ? bonds_() : std::vector<BondInfo>{};
    send(Msg::Bonds, device_config::serialize_bonds(bonds));
  }

  send_fn send_;
  settings_changed_fn on_settings_changed_;
  info_fn info_;
  action_fn on_action_;
  bonds_fn bonds_;
  forget_bond_fn forget_bond_;

  mutable std::mutex mutex_;
  Settings settings_;
};
