#pragma once

#include "usb_manager/registry/DeviceRegistry.hpp"
#include "usb_manager/session/SessionOrchestrator.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace usb_manager {

/**
 * D-Bus service: org.example.connectivity.Usb1
 */
class UsbDbusService {
 public:
  UsbDbusService(DeviceRegistry& registry, SessionOrchestrator& orchestrator);
  ~UsbDbusService();

  bool start();
  void stop();
  void emitDeviceChanged(const UsbDeviceInfo& device);
  void emitSessionChanged(const SessionInfo& session);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace usb_manager
