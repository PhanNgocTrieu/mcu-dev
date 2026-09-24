#pragma once

#include "usb_manager/Types.hpp"

namespace usb_manager {

/**
 * P0: Classify USB devices from udev properties / VID:PID heuristics.
 */
class DeviceClassifier {
 public:
  DeviceType classify(const UsbDeviceInfo& info) const;

  /** Fill manufacturer/product hints and type. */
  void enrich(UsbDeviceInfo& info) const;

 private:
  static bool isApple(uint16_t vid);
  static bool looksLikeAndroid(const UsbDeviceInfo& info);
  static bool looksLikeMassStorage(const UsbDeviceInfo& info);
  static bool looksLikeHid(const UsbDeviceInfo& info);
};

}  // namespace usb_manager
