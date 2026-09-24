#include "usb_manager/classify/DeviceClassifier.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace usb_manager {
namespace {

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool containsCi(const std::string& hay, const char* needle) {
  return toLower(hay).find(toLower(needle)) != std::string::npos;
}

}  // namespace

bool DeviceClassifier::isApple(uint16_t vid) { return vid == 0x05ac; }

bool DeviceClassifier::looksLikeMassStorage(const UsbDeviceInfo& info) {
  // USB class 08 = Mass Storage often encoded in ID_USB_INTERFACES as :0806xx:
  if (info.interfaces.find(":08") != std::string::npos) return true;
  if (containsCi(info.product, "USB Disk") || containsCi(info.product, "Mass Storage")) {
    return true;
  }
  return false;
}

bool DeviceClassifier::looksLikeHid(const UsbDeviceInfo& info) {
  return info.interfaces.find(":03") != std::string::npos;
}

bool DeviceClassifier::looksLikeAndroid(const UsbDeviceInfo& info) {
  if (isApple(info.vendorId)) return false;
  // Common Android interface patterns: MTP (ff), ADB (ff), accessory later
  if (containsCi(info.manufacturer, "Google") || containsCi(info.manufacturer, "Android") ||
      containsCi(info.product, "Android") || containsCi(info.product, "MTP") ||
      containsCi(info.product, "ADB")) {
    return true;
  }
  // Many phones expose MTP (class 0xFF vendor-specific) — treat non-Apple phone-like as Android candidate
  if (!info.serial.empty() && info.interfaces.find(":06") != std::string::npos) {
    // Still could be camera PTP; prefer manufacturer hints
  }
  if (info.interfaces.find(":ff") != std::string::npos ||
      info.interfaces.find(":FF") != std::string::npos) {
    return true;
  }
  // Known-ish phone VIDs (non-exhaustive, for prototype learning)
  switch (info.vendorId) {
    case 0x18d1:  // Google
    case 0x04e8:  // Samsung
    case 0x22b8:  // Motorola
    case 0x0bb4:  // HTC
    case 0x12d1:  // Huawei
    case 0x2717:  // Xiaomi
    case 0x2a47:  // Nothing / Oppo-ish
    case 0x05c6:  // Qualcomm (many OEMs)
      return true;
    default:
      break;
  }
  return false;
}

DeviceType DeviceClassifier::classify(const UsbDeviceInfo& info) const {
  if (isApple(info.vendorId)) {
    return DeviceType::IPhone;
  }
  if (looksLikeAndroid(info)) {
    return DeviceType::Android;
  }
  if (looksLikeMassStorage(info) && !looksLikeAndroid(info)) {
    return DeviceType::MassStorage;
  }
  if (looksLikeHid(info)) {
    return DeviceType::Hid;
  }
  return DeviceType::Unknown;
}

void DeviceClassifier::enrich(UsbDeviceInfo& info) const {
  info.type = classify(info);
}

}  // namespace usb_manager
