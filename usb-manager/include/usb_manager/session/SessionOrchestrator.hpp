#pragma once

#include "usb_manager/Types.hpp"
#include "usb_manager/adapters/IProjectionAdapter.hpp"
#include "usb_manager/policy/ConnectionPolicy.hpp"
#include "usb_manager/registry/DeviceRegistry.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace usb_manager {

using SessionChangeCallback =
    std::function<void(const SessionInfo& session)>;

/**
 * Device + session lifecycle orchestrator (plan state machine).
 */
class SessionOrchestrator {
 public:
  SessionOrchestrator(DeviceRegistry& registry, ConnectionPolicy& policy);

  void setAndroidAutoAdapter(std::shared_ptr<IProjectionAdapter> adapter);
  void setCarPlayAdapter(std::shared_ptr<IProjectionAdapter> adapter);
  void setSessionChangeCallback(SessionChangeCallback cb);

  void onDeviceAdded(UsbDeviceInfo device);
  void onDeviceRemoved(const std::string& deviceId);

  /** Probe transports (AOAP/NCM) after classification. */
  void probeDevice(const std::string& deviceId);

  bool startSession(const std::string& deviceId, SessionMode mode,
                    std::string& errorOut);
  bool stopSession(const std::string& deviceId, std::string& errorOut);

  std::optional<SessionInfo> activeSession() const;

 private:
  void emitSession(const SessionInfo& info);
  void transition(UsbDeviceInfo& device, DeviceState next,
                  const std::string& reason);
  std::shared_ptr<IProjectionAdapter> adapterFor(SessionMode mode) const;

  DeviceRegistry& registry_;
  ConnectionPolicy& policy_;
  std::shared_ptr<IProjectionAdapter> aa_;
  std::shared_ptr<IProjectionAdapter> cp_;
  SessionChangeCallback onSession_;
  SessionInfo active_;
};

}  // namespace usb_manager
