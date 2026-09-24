#pragma once

#include "usb_manager/Types.hpp"

namespace usb_manager {

struct AoapProbeResult {
  bool attempted{false};
  bool supported{false};
  std::string detail;
};

struct NcmProbeResult {
  bool ifacePresent{false};
  std::string ifName;
  std::string detail;
};

/**
 * P2: Probe AOAP (libusb control) and CDC-NCM/RNDIS net interfaces.
 */
class TransportProber {
 public:
  AoapProbeResult probeAoap(const UsbDeviceInfo& device) const;
  NcmProbeResult probeNcm(const UsbDeviceInfo& device) const;
};

}  // namespace usb_manager
