#include "usb_manager/adapters/AndroidAutoAdapter.hpp"
#include "usb_manager/aoap/TransportProber.hpp"
#include "usb_manager/util/Log.hpp"

namespace usb_manager {

AndroidAutoAdapter::AndroidAutoAdapter(std::shared_ptr<IAasdkSession> session)
    : session_(std::move(session)) {}

SessionMode AndroidAutoAdapter::mode() const { return SessionMode::AndroidAuto; }

std::string AndroidAutoAdapter::name() const {
  return std::string("AndroidAutoAdapter/") + (session_ ? session_->name() : "none");
}

AdapterResult AndroidAutoAdapter::probe(UsbDeviceInfo& device) {
  TransportProber prober;
  auto aoap = prober.probeAoap(device);
  auto ncm = prober.probeNcm(device);
  device.aoapSupported = aoap.supported;
  device.ncmIfacePresent = ncm.ifacePresent;
  device.netInterface = ncm.ifName;

  AdapterResult r;
  r.ok = aoap.supported || ncm.ifacePresent;
  r.message = "aoap=" + aoap.detail + "; ncm=" + ncm.detail;
  if (!r.ok) {
    // Still allow demo session for learning when phone is classified Android
    r.ok = (device.type == DeviceType::Android);
    if (r.ok) {
      r.message += "; proceed_demo_without_transport";
      USB_LOG_WARN("AA probe: no AOAP/NCM yet — allowing demo path for " + device.deviceId);
    }
  }
  return r;
}

AdapterResult AndroidAutoAdapter::start(const UsbDeviceInfo& device) {
  if (!session_) return {false, "no_aasdk_session"};
  std::string err;
  if (!session_->start(device, err)) {
    return {false, err};
  }
  return {true, "aa_session_active:" + session_->name()};
}

AdapterResult AndroidAutoAdapter::stop(const UsbDeviceInfo& device) {
  (void)device;
  if (session_) session_->stop();
  return {true, "aa_session_stopped"};
}

}  // namespace usb_manager
