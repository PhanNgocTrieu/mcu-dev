/**
 * Offline unit checks for classifier + policy (no root/USB required).
 */
#include "usb_manager/classify/DeviceClassifier.hpp"
#include "usb_manager/policy/ConnectionPolicy.hpp"
#include "usb_manager/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace usb_manager;

static int gFailures = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++gFailures;
  } else {
    std::printf("OK: %s\n", msg);
  }
}

int main() {
  DeviceClassifier clf;

  UsbDeviceInfo apple;
  apple.vendorId = 0x05ac;
  apple.productId = 0x12a8;
  apple.product = "iPhone";
  expect(clf.classify(apple) == DeviceType::IPhone, "Apple VID -> IPhone");

  UsbDeviceInfo google;
  google.vendorId = 0x18d1;
  google.productId = 0x4ee1;
  google.manufacturer = "Google";
  google.product = "Pixel";
  expect(clf.classify(google) == DeviceType::Android, "Google -> Android");

  UsbDeviceInfo samsung;
  samsung.vendorId = 0x04e8;
  samsung.interfaces = ":ff4201:";
  expect(clf.classify(samsung) == DeviceType::Android, "Samsung VID -> Android");

  UsbDeviceInfo stick;
  stick.vendorId = 0x0781;
  stick.interfaces = ":080650:";
  stick.product = "USB Disk";
  expect(clf.classify(stick) == DeviceType::MassStorage, "Mass storage class");

  ConnectionPolicy policy;
  UsbDeviceInfo a = google;
  a.type = DeviceType::Android;
  a.state = DeviceState::Ready;
  a.deviceId = "usb-1-2";
  a.serial = "ABC";

  UsbDeviceInfo b = apple;
  b.type = DeviceType::IPhone;
  b.state = DeviceState::Ready;
  b.deviceId = "usb-1-3";
  b.serial = "XYZ";

  auto d1 = policy.canStart(a, SessionMode::AndroidAuto, std::nullopt);
  expect(d1.allowed, "AA start allowed when idle");

  SessionInfo active{a.deviceId, SessionMode::AndroidAuto, SessionState::Active, "ok"};
  auto d2 = policy.canStart(b, SessionMode::CarPlay, active);
  expect(!d2.allowed, "exclusive: reject CarPlay while AA active");
  expect(d2.reason.find("exclusive") != std::string::npos, "exclusive reason string");

  policy.setAllowlistEnabled(true);
  policy.allowSerial("ABC");
  auto d3 = policy.canStart(a, SessionMode::AndroidAuto, std::nullopt);
  expect(d3.allowed, "allowlist permits ABC");
  auto d4 = policy.canStart(b, SessionMode::CarPlay, std::nullopt);
  expect(!d4.allowed, "allowlist rejects XYZ");

  // CarPlay stub contract: preferred mode
  expect(policy.preferredMode(b) == SessionMode::CarPlay, "iPhone prefers CarPlay");

  if (gFailures) {
    std::fprintf(stderr, "%d failures\n", gFailures);
    return 1;
  }
  std::printf("All checks passed\n");
  return 0;
}
