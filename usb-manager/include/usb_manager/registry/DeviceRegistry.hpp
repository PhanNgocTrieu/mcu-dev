#pragma once

#include "usb_manager/Types.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace usb_manager {

using DeviceChangeCallback =
    std::function<void(const UsbDeviceInfo& device, const std::string& reason)>;

class DeviceRegistry {
 public:
  void setChangeCallback(DeviceChangeCallback cb);

  void upsert(UsbDeviceInfo device, const std::string& reason = "update");
  bool remove(const std::string& deviceId, const std::string& reason = "remove");
  std::optional<UsbDeviceInfo> get(const std::string& deviceId) const;
  std::vector<UsbDeviceInfo> list() const;
  bool updateState(const std::string& deviceId, DeviceState state,
                   const std::string& reason = "state");
  bool updateFields(const std::string& deviceId,
                    const std::function<void(UsbDeviceInfo&)>& mutator,
                    const std::string& reason = "update");

 private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, UsbDeviceInfo> devices_;
  DeviceChangeCallback onChange_;
};

}  // namespace usb_manager
