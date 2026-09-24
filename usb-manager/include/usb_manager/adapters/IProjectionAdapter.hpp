#pragma once

#include "usb_manager/Types.hpp"

#include <string>

namespace usb_manager {

struct AdapterResult {
  bool ok{false};
  std::string message;
};

/**
 * Projection stack boundary (AA / CarPlay).
 */
class IProjectionAdapter {
 public:
  virtual ~IProjectionAdapter() = default;

  virtual SessionMode mode() const = 0;
  virtual std::string name() const = 0;

  /** Optional transport probe before session start. */
  virtual AdapterResult probe(UsbDeviceInfo& device) = 0;

  virtual AdapterResult start(const UsbDeviceInfo& device) = 0;
  virtual AdapterResult stop(const UsbDeviceInfo& device) = 0;
};

}  // namespace usb_manager
