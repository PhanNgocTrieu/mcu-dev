/**
 * P0 lab tool: print USB add/remove + VID/PID classification.
 * Usage: usb-udev-lab [--once]
 */
#include "usb_manager/classify/DeviceClassifier.hpp"
#include "usb_manager/hotplug/UdevMonitor.hpp"
#include "usb_manager/util/Log.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>

namespace {
std::atomic<bool> gRun{true};
void onSig(int) { gRun = false; }
}  // namespace

int main(int argc, char** argv) {
  using namespace usb_manager;
  bool once = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--once") once = true;
  }

  std::signal(SIGINT, onSig);
  logSetLevel(LogLevel::Info);

  UdevMonitor mon;
  mon.setCallback([](const HotplugEvent& ev) {
    const char* act =
        ev.action == HotplugAction::Remove ? "REMOVE" :
        ev.action == HotplugAction::Change ? "CHANGE" : "ADD";
    std::printf(
        "%-6s id=%-16s type=%-12s %04x:%04x mfr=\"%s\" product=\"%s\" serial=\"%s\" "
        "ifaces=\"%s\"\n",
        act, ev.device.deviceId.c_str(), toString(ev.device.type), ev.device.vendorId,
        ev.device.productId, ev.device.manufacturer.c_str(), ev.device.product.c_str(),
        ev.device.serial.c_str(), ev.device.interfaces.c_str());
  });

  if (!mon.start()) {
    std::cerr << "Failed to start udev monitor\n";
    return 1;
  }
  mon.enumerateExisting();
  if (once) return 0;

  std::cerr << "Listening for USB plug/unplug... Ctrl+C to quit\n";
  while (gRun.load()) {
    mon.pollOnce(500);
  }
  return 0;
}
