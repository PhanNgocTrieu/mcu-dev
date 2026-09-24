#include "usb_manager/adapters/CarPlayStubAdapter.hpp"
#include "usb_manager/util/Log.hpp"

namespace usb_manager {

SessionMode CarPlayStubAdapter::mode() const { return SessionMode::CarPlay; }

std::string CarPlayStubAdapter::name() const { return "CarPlayStubAdapter"; }

AdapterResult CarPlayStubAdapter::probe(UsbDeviceInfo& device) {
  if (device.type != DeviceType::IPhone && device.vendorId != 0x05ac) {
    return {false, "not_apple_device"};
  }
  USB_LOG_INFO("CarPlay stub: Apple device detected " + device.deviceId +
               " — Available=false Reason=MFI_REQUIRED");
  return {true, "detected_apple_stub_only"};
}

AdapterResult CarPlayStubAdapter::start(const UsbDeviceInfo& device) {
  (void)device;
  USB_LOG_WARN("CarPlay stub: start rejected — MFI_REQUIRED (need Apple MFi + licensed stack)");
  return {false, "MFI_REQUIRED"};
}

AdapterResult CarPlayStubAdapter::stop(const UsbDeviceInfo& device) {
  (void)device;
  return {true, "carplay_stub_idle"};
}

}  // namespace usb_manager
