#include "usb_manager/registry/DeviceRegistry.hpp"

namespace usb_manager {

void DeviceRegistry::setChangeCallback(DeviceChangeCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  onChange_ = std::move(cb);
}

void DeviceRegistry::upsert(UsbDeviceInfo device, const std::string& reason) {
  DeviceChangeCallback cb;
  UsbDeviceInfo copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    devices_[device.deviceId] = device;
    copy = device;
    cb = onChange_;
  }
  if (cb) cb(copy, reason);
}

bool DeviceRegistry::remove(const std::string& deviceId, const std::string& reason) {
  DeviceChangeCallback cb;
  UsbDeviceInfo copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) return false;
    copy = it->second;
    copy.state = DeviceState::Idle;
    devices_.erase(it);
    cb = onChange_;
  }
  if (cb) cb(copy, reason);
  return true;
}

std::optional<UsbDeviceInfo> DeviceRegistry::get(const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = devices_.find(deviceId);
  if (it == devices_.end()) return std::nullopt;
  return it->second;
}

std::vector<UsbDeviceInfo> DeviceRegistry::list() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<UsbDeviceInfo> out;
  out.reserve(devices_.size());
  for (const auto& kv : devices_) out.push_back(kv.second);
  return out;
}

bool DeviceRegistry::updateState(const std::string& deviceId, DeviceState state,
                                 const std::string& reason) {
  return updateFields(
      deviceId,
      [state](UsbDeviceInfo& d) { d.state = state; },
      reason);
}

bool DeviceRegistry::updateFields(const std::string& deviceId,
                                  const std::function<void(UsbDeviceInfo&)>& mutator,
                                  const std::string& reason) {
  DeviceChangeCallback cb;
  UsbDeviceInfo copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) return false;
    mutator(it->second);
    copy = it->second;
    cb = onChange_;
  }
  if (cb) cb(copy, reason);
  return true;
}

}  // namespace usb_manager
