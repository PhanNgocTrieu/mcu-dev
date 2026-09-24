#include "usb_manager/policy/ConnectionPolicy.hpp"

namespace usb_manager {

void ConnectionPolicy::setAllowlistEnabled(bool enabled) { allowlistEnabled_ = enabled; }

void ConnectionPolicy::allowSerial(const std::string& serial) {
  if (!serial.empty()) allowlist_.insert(serial);
}

void ConnectionPolicy::clearAllowlist() { allowlist_.clear(); }

SessionMode ConnectionPolicy::preferredMode(const UsbDeviceInfo& device) const {
  switch (device.type) {
    case DeviceType::Android:
      return SessionMode::AndroidAuto;
    case DeviceType::IPhone:
      return SessionMode::CarPlay;
    case DeviceType::MassStorage:
      return SessionMode::Storage;
    default:
      return SessionMode::None;
  }
}

PolicyDecision ConnectionPolicy::canStart(const UsbDeviceInfo& device, SessionMode mode,
                                          const std::optional<SessionInfo>& active) const {
  if (mode == SessionMode::None) {
    return {false, "invalid_mode"};
  }
  if (device.state == DeviceState::Ignored || device.state == DeviceState::Failed) {
    return {false, "device_not_ready"};
  }
  if (allowlistEnabled_) {
    if (device.serial.empty() || allowlist_.count(device.serial) == 0) {
      return {false, "not_in_allowlist"};
    }
  }
  // Exclusive projection: only one AA/CarPlay session at a time
  if (mode == SessionMode::AndroidAuto || mode == SessionMode::CarPlay) {
    if (active && active->state == SessionState::Active &&
        (active->mode == SessionMode::AndroidAuto || active->mode == SessionMode::CarPlay) &&
        active->deviceId != device.deviceId) {
      return {false, "exclusive_session_active:" + active->deviceId};
    }
  }
  if (mode == SessionMode::AndroidAuto && device.type != DeviceType::Android) {
    return {false, "type_mismatch_android"};
  }
  if (mode == SessionMode::CarPlay && device.type != DeviceType::IPhone) {
    return {false, "type_mismatch_iphone"};
  }
  if (mode == SessionMode::Storage && device.type != DeviceType::MassStorage) {
    return {false, "type_mismatch_storage"};
  }
  return {true, "ok"};
}

}  // namespace usb_manager
