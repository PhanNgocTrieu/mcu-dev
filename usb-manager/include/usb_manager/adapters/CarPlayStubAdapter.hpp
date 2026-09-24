#pragma once

#include "usb_manager/adapters/IProjectionAdapter.hpp"

namespace usb_manager {

/**
 * P4: CarPlay stub — detects Apple devices, reports MFI_REQUIRED.
 */
class CarPlayStubAdapter : public IProjectionAdapter {
 public:
  SessionMode mode() const override;
  std::string name() const override;
  AdapterResult probe(UsbDeviceInfo& device) override;
  AdapterResult start(const UsbDeviceInfo& device) override;
  AdapterResult stop(const UsbDeviceInfo& device) override;

  bool available() const { return false; }
  std::string unavailableReason() const { return "MFI_REQUIRED"; }
};

}  // namespace usb_manager
