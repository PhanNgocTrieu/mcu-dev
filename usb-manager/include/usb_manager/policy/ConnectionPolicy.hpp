#pragma once

#include "usb_manager/Types.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace usb_manager {

struct PolicyDecision {
  bool allowed{false};
  std::string reason;
};

/**
 * P4: Exclusive projection session + optional allowlist.
 */
class ConnectionPolicy {
 public:
  void setAllowlistEnabled(bool enabled);
  void allowSerial(const std::string& serial);
  void clearAllowlist();

  PolicyDecision canStart(const UsbDeviceInfo& device, SessionMode mode,
                          const std::optional<SessionInfo>& active) const;

  SessionMode preferredMode(const UsbDeviceInfo& device) const;

 private:
  bool allowlistEnabled_{false};
  std::unordered_set<std::string> allowlist_;
};

}  // namespace usb_manager
