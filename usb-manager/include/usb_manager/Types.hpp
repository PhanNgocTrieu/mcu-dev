#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usb_manager {

enum class DeviceType {
  Unknown,
  Android,
  IPhone,
  MassStorage,
  Hid,
};

enum class DeviceState {
  Idle,
  Enumerating,
  Classified,
  Probing,
  Ready,
  Connecting,
  Active,
  Failed,
  Disconnecting,
  Ignored,
};

enum class SessionMode {
  None,
  AndroidAuto,
  CarPlay,
  Storage,
};

enum class SessionState {
  Idle,
  Starting,
  Active,
  Stopping,
  Failed,
};

inline const char* toString(DeviceType t) {
  switch (t) {
    case DeviceType::Unknown: return "unknown";
    case DeviceType::Android: return "android";
    case DeviceType::IPhone: return "iphone";
    case DeviceType::MassStorage: return "mass_storage";
    case DeviceType::Hid: return "hid";
  }
  return "unknown";
}

inline const char* toString(DeviceState s) {
  switch (s) {
    case DeviceState::Idle: return "idle";
    case DeviceState::Enumerating: return "enumerating";
    case DeviceState::Classified: return "classified";
    case DeviceState::Probing: return "probing";
    case DeviceState::Ready: return "ready";
    case DeviceState::Connecting: return "connecting";
    case DeviceState::Active: return "active";
    case DeviceState::Failed: return "failed";
    case DeviceState::Disconnecting: return "disconnecting";
    case DeviceState::Ignored: return "ignored";
  }
  return "idle";
}

inline const char* toString(SessionMode m) {
  switch (m) {
    case SessionMode::None: return "none";
    case SessionMode::AndroidAuto: return "android_auto";
    case SessionMode::CarPlay: return "carplay";
    case SessionMode::Storage: return "storage";
  }
  return "none";
}

inline const char* toString(SessionState s) {
  switch (s) {
    case SessionState::Idle: return "idle";
    case SessionState::Starting: return "starting";
    case SessionState::Active: return "active";
    case SessionState::Stopping: return "stopping";
    case SessionState::Failed: return "failed";
  }
  return "idle";
}

inline std::optional<SessionMode> sessionModeFromString(const std::string& s) {
  if (s == "android_auto") return SessionMode::AndroidAuto;
  if (s == "carplay") return SessionMode::CarPlay;
  if (s == "storage") return SessionMode::Storage;
  return std::nullopt;
}

struct UsbDeviceInfo {
  std::string deviceId;      // stable id: busnum-devnum or syspath hash
  std::string sysPath;
  std::string devNode;
  uint16_t vendorId{0};
  uint16_t productId{0};
  std::string manufacturer;
  std::string product;
  std::string serial;
  std::string interfaces;    // ID_USB_INTERFACES
  DeviceType type{DeviceType::Unknown};
  DeviceState state{DeviceState::Idle};
  std::string lastError;
  bool aoapSupported{false};
  bool ncmIfacePresent{false};
  std::string netInterface;  // e.g. usb0
};

struct SessionInfo {
  std::string deviceId;
  SessionMode mode{SessionMode::None};
  SessionState state{SessionState::Idle};
  std::string reason;
};

}  // namespace usb_manager
