#pragma once

#include "usb_manager/Types.hpp"

#include <string>

namespace usb_manager {

/**
 * Minimal AASDK session boundary.
 * DemoAasdkSession simulates channels; RealAasdkSession would wrap f1xpl/aasdk.
 */
class IAasdkSession {
 public:
  virtual ~IAasdkSession() = default;
  virtual std::string name() const = 0;
  virtual bool start(const UsbDeviceInfo& device, std::string& errorOut) = 0;
  virtual void stop() = 0;
  virtual bool isActive() const = 0;
};

class DemoAasdkSession : public IAasdkSession {
 public:
  std::string name() const override;
  bool start(const UsbDeviceInfo& device, std::string& errorOut) override;
  void stop() override;
  bool isActive() const override;

 private:
  bool active_{false};
  std::string deviceId_;
};

}  // namespace usb_manager
