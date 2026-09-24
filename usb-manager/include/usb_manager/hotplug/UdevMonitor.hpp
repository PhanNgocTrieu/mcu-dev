#pragma once

#include "usb_manager/Types.hpp"

#include <functional>
#include <memory>
#include <string>

namespace usb_manager {

enum class HotplugAction { Add, Remove, Change };

struct HotplugEvent {
  HotplugAction action{HotplugAction::Add};
  UsbDeviceInfo device;
};

using HotplugCallback = std::function<void(const HotplugEvent&)>;

/**
 * P0: Monitor USB device add/remove via libudev netlink.
 */
class UdevMonitor {
 public:
  UdevMonitor();
  ~UdevMonitor();

  UdevMonitor(const UdevMonitor&) = delete;
  UdevMonitor& operator=(const UdevMonitor&) = delete;

  void setCallback(HotplugCallback cb);
  bool start();
  void stop();

  /** Poll once (non-blocking). Returns true if an event was handled. */
  bool pollOnce(int timeoutMs = 0);

  /** Enumerate currently attached USB devices and emit synthetic Add events. */
  void enumerateExisting();

  int fd() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace usb_manager
