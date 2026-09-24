#include "usb_manager/adapters/AndroidAutoAdapter.hpp"
#include "usb_manager/adapters/CarPlayStubAdapter.hpp"
#include "usb_manager/adapters/IAasdkSession.hpp"
#include "usb_manager/hotplug/UdevMonitor.hpp"
#include "usb_manager/ipc/UsbDbusService.hpp"
#include "usb_manager/policy/ConnectionPolicy.hpp"
#include "usb_manager/registry/DeviceRegistry.hpp"
#include "usb_manager/session/SessionOrchestrator.hpp"
#include "usb_manager/util/Log.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

std::atomic<bool> gRunning{true};

void onSignal(int) { gRunning = false; }

void printUsage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " [--no-dbus] [--no-auto-enum] [--allowlist SERIAL] [--debug]\n"
      << "  USB Manager daemon (Host) for Connectivity SC prototype.\n";
}

}  // namespace

int main(int argc, char** argv) {
  using namespace usb_manager;

  bool enableDbus = true;
  bool autoEnum = true;
  std::string allowSerial;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      printUsage(argv[0]);
      return 0;
    } else if (a == "--no-dbus") {
      enableDbus = false;
    } else if (a == "--no-auto-enum") {
      autoEnum = false;
    } else if (a == "--debug") {
      logSetLevel(LogLevel::Debug);
    } else if (a == "--allowlist" && i + 1 < argc) {
      allowSerial = argv[++i];
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      printUsage(argv[0]);
      return 2;
    }
  }

  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  DeviceRegistry registry;
  ConnectionPolicy policy;
  if (!allowSerial.empty()) {
    policy.setAllowlistEnabled(true);
    policy.allowSerial(allowSerial);
    USB_LOG_INFO("Allowlist enabled for serial=" + allowSerial);
  }

  SessionOrchestrator orchestrator(registry, policy);
  auto aasdk = std::make_shared<DemoAasdkSession>();
  orchestrator.setAndroidAutoAdapter(std::make_shared<AndroidAutoAdapter>(aasdk));
  orchestrator.setCarPlayAdapter(std::make_shared<CarPlayStubAdapter>());

  std::unique_ptr<UsbDbusService> dbus;
  if (enableDbus) {
    dbus = std::make_unique<UsbDbusService>(registry, orchestrator);
    registry.setChangeCallback([&](const UsbDeviceInfo& d, const std::string&) {
      if (dbus) dbus->emitDeviceChanged(d);
    });
    orchestrator.setSessionChangeCallback([&](const SessionInfo& s) {
      if (dbus) dbus->emitSessionChanged(s);
    });
    dbus->start();
  }

  UdevMonitor monitor;
  monitor.setCallback([&](const HotplugEvent& ev) {
    if (ev.action == HotplugAction::Remove) {
      orchestrator.onDeviceRemoved(ev.device.deviceId);
    } else {
      orchestrator.onDeviceAdded(ev.device);
    }
  });

  if (!monitor.start()) {
    USB_LOG_ERROR("Failed to start UdevMonitor");
    return 1;
  }
  if (autoEnum) {
    monitor.enumerateExisting();
  }

  USB_LOG_INFO("usb-manager running (Host mode). Ctrl+C to stop.");
  while (gRunning.load()) {
    monitor.pollOnce(500);
  }

  USB_LOG_INFO("Shutting down...");
  if (dbus) dbus->stop();
  monitor.stop();
  return 0;
}
