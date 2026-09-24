#include "usb_manager/adapters/IAasdkSession.hpp"
#include "usb_manager/util/Log.hpp"

namespace usb_manager {

std::string DemoAasdkSession::name() const { return "DemoAasdkSession"; }

bool DemoAasdkSession::start(const UsbDeviceInfo& device, std::string& errorOut) {
  if (active_) {
    errorOut = "already_active";
    return false;
  }
  deviceId_ = device.deviceId;
  active_ = true;
  // Simulate AASDK channel bring-up (video / audio / input / sensor)
  USB_LOG_INFO("AASDK demo: session start device=" + deviceId_);
  USB_LOG_INFO("AASDK demo: open VideoChannel (H264) — wire to HMI surface");
  USB_LOG_INFO("AASDK demo: open AudioChannel — route to Audio SC");
  USB_LOG_INFO("AASDK demo: open InputChannel — touch/keys from HMI");
  USB_LOG_INFO("AASDK demo: open SensorChannel — night mode / location stubs");
  USB_LOG_INFO(
      "AASDK demo: replace DemoAasdkSession with real f1xpl/aasdk "
      "(see docs/AASDK_INTEGRATION.md)");
  return true;
}

void DemoAasdkSession::stop() {
  if (!active_) return;
  USB_LOG_INFO("AASDK demo: session stop device=" + deviceId_);
  active_ = false;
  deviceId_.clear();
}

bool DemoAasdkSession::isActive() const { return active_; }

}  // namespace usb_manager
