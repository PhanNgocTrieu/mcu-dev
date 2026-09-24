#pragma once

#include "usb_manager/adapters/IProjectionAdapter.hpp"
#include "usb_manager/adapters/IAasdkSession.hpp"

#include <memory>

namespace usb_manager {

/**
 * P2/P3: Android Auto adapter — AOAP/NCM probe + AASDK session (demo by default).
 */
class AndroidAutoAdapter : public IProjectionAdapter {
 public:
  explicit AndroidAutoAdapter(std::shared_ptr<IAasdkSession> session);

  SessionMode mode() const override;
  std::string name() const override;
  AdapterResult probe(UsbDeviceInfo& device) override;
  AdapterResult start(const UsbDeviceInfo& device) override;
  AdapterResult stop(const UsbDeviceInfo& device) override;

 private:
  std::shared_ptr<IAasdkSession> session_;
};

}  // namespace usb_manager
