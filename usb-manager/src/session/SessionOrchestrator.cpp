#include "usb_manager/session/SessionOrchestrator.hpp"
#include "usb_manager/util/Log.hpp"

namespace usb_manager {

SessionOrchestrator::SessionOrchestrator(DeviceRegistry& registry, ConnectionPolicy& policy)
    : registry_(registry), policy_(policy) {
  active_.state = SessionState::Idle;
  active_.mode = SessionMode::None;
}

void SessionOrchestrator::setAndroidAutoAdapter(std::shared_ptr<IProjectionAdapter> adapter) {
  aa_ = std::move(adapter);
}

void SessionOrchestrator::setCarPlayAdapter(std::shared_ptr<IProjectionAdapter> adapter) {
  cp_ = std::move(adapter);
}

void SessionOrchestrator::setSessionChangeCallback(SessionChangeCallback cb) {
  onSession_ = std::move(cb);
}

void SessionOrchestrator::emitSession(const SessionInfo& info) {
  active_ = info;
  if (onSession_) onSession_(info);
}

void SessionOrchestrator::transition(UsbDeviceInfo& device, DeviceState next,
                                     const std::string& reason) {
  USB_LOG_INFO(std::string("state ") + device.deviceId + " " + toString(device.state) + " -> " +
               toString(next) + " (" + reason + ")");
  device.state = next;
  registry_.upsert(device, reason);
}

std::shared_ptr<IProjectionAdapter> SessionOrchestrator::adapterFor(SessionMode mode) const {
  switch (mode) {
    case SessionMode::AndroidAuto:
      return aa_;
    case SessionMode::CarPlay:
      return cp_;
    default:
      return nullptr;
  }
}

std::optional<SessionInfo> SessionOrchestrator::activeSession() const {
  if (active_.state == SessionState::Idle || active_.mode == SessionMode::None) {
    return std::nullopt;
  }
  return active_;
}

void SessionOrchestrator::onDeviceAdded(UsbDeviceInfo device) {
  device.state = DeviceState::Enumerating;
  registry_.upsert(device, "add");

  if (device.type == DeviceType::Unknown || device.type == DeviceType::Hid) {
    device.state = DeviceState::Ignored;
    registry_.upsert(device, "ignored");
    return;
  }

  transition(device, DeviceState::Classified, std::string("classified:") + toString(device.type));
  probeDevice(device.deviceId);
}

void SessionOrchestrator::onDeviceRemoved(const std::string& deviceId) {
  auto existing = registry_.get(deviceId);
  if (!existing) return;

  if (active_.deviceId == deviceId &&
      (active_.state == SessionState::Active || active_.state == SessionState::Starting)) {
    std::string err;
    stopSession(deviceId, err);
  }

  UsbDeviceInfo d = *existing;
  transition(d, DeviceState::Disconnecting, "udev_remove");
  registry_.remove(deviceId, "removed");
}

void SessionOrchestrator::probeDevice(const std::string& deviceId) {
  auto opt = registry_.get(deviceId);
  if (!opt) return;
  UsbDeviceInfo device = *opt;
  transition(device, DeviceState::Probing, "probe_start");

  auto mode = policy_.preferredMode(device);
  auto adapter = adapterFor(mode);
  if (!adapter) {
    // Mass storage / no adapter — mark ready for storage or ignore
    if (device.type == DeviceType::MassStorage) {
      transition(device, DeviceState::Ready, "storage_ready");
    } else {
      transition(device, DeviceState::Ignored, "no_adapter");
    }
    return;
  }

  auto result = adapter->probe(device);
  registry_.updateFields(
      deviceId,
      [&](UsbDeviceInfo& d) {
        d.aoapSupported = device.aoapSupported;
        d.ncmIfacePresent = device.ncmIfacePresent;
        d.netInterface = device.netInterface;
        d.lastError = result.ok ? "" : result.message;
      },
      "probe_fields");

  opt = registry_.get(deviceId);
  if (!opt) return;
  device = *opt;

  if (result.ok) {
    transition(device, DeviceState::Ready, result.message);
    // Auto-start policy for AA when ready (prototype learning default)
    if (mode == SessionMode::AndroidAuto) {
      std::string err;
      startSession(deviceId, SessionMode::AndroidAuto, err);
    } else if (mode == SessionMode::CarPlay) {
      // Stub: publish availability via session failed signal (HMI can show message)
      SessionInfo s;
      s.deviceId = deviceId;
      s.mode = SessionMode::CarPlay;
      s.state = SessionState::Failed;
      s.reason = "MFI_REQUIRED";
      emitSession(s);
      USB_LOG_INFO("CarPlay available=false for " + deviceId);
    }
  } else {
    device.lastError = result.message;
    transition(device, DeviceState::Failed, result.message);
  }
}

bool SessionOrchestrator::startSession(const std::string& deviceId, SessionMode mode,
                                       std::string& errorOut) {
  auto opt = registry_.get(deviceId);
  if (!opt) {
    errorOut = "device_not_found";
    return false;
  }
  UsbDeviceInfo device = *opt;

  auto decision = policy_.canStart(device, mode, activeSession());
  if (!decision.allowed) {
    errorOut = decision.reason;
    USB_LOG_WARN("policy deny start " + deviceId + ": " + errorOut);
    return false;
  }

  auto adapter = adapterFor(mode);
  if (!adapter && mode != SessionMode::Storage) {
    errorOut = "no_adapter";
    return false;
  }

  transition(device, DeviceState::Connecting, "session_start");
  SessionInfo starting;
  starting.deviceId = deviceId;
  starting.mode = mode;
  starting.state = SessionState::Starting;
  starting.reason = "starting";
  emitSession(starting);

  if (mode == SessionMode::Storage) {
    transition(device, DeviceState::Active, "storage_mounted_stub");
    SessionInfo s{deviceId, mode, SessionState::Active, "storage_ok"};
    emitSession(s);
    return true;
  }

  auto result = adapter->start(device);
  opt = registry_.get(deviceId);
  if (opt) device = *opt;

  if (!result.ok) {
    errorOut = result.message;
    transition(device, DeviceState::Failed, result.message);
    SessionInfo s{deviceId, mode, SessionState::Failed, result.message};
    emitSession(s);
    return false;
  }

  transition(device, DeviceState::Active, result.message);
  SessionInfo s{deviceId, mode, SessionState::Active, result.message};
  emitSession(s);
  return true;
}

bool SessionOrchestrator::stopSession(const std::string& deviceId, std::string& errorOut) {
  auto opt = registry_.get(deviceId);
  if (!opt) {
    errorOut = "device_not_found";
    return false;
  }
  UsbDeviceInfo device = *opt;
  SessionMode mode = active_.deviceId == deviceId ? active_.mode : policy_.preferredMode(device);
  auto adapter = adapterFor(mode);

  SessionInfo stopping{deviceId, mode, SessionState::Stopping, "stopping"};
  emitSession(stopping);

  if (adapter) {
    adapter->stop(device);
  }

  auto after = registry_.get(deviceId);
  if (after) {
    transition(*after, DeviceState::Ready, "session_stopped");
  }

  SessionInfo idle;
  idle.deviceId.clear();
  idle.mode = SessionMode::None;
  idle.state = SessionState::Idle;
  idle.reason = "stopped";
  emitSession(idle);
  return true;
}

}  // namespace usb_manager
