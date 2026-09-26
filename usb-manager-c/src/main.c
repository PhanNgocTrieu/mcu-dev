#include "adapter.h"
#include "dbus_api.h"
#include "log.h"
#include "orchestrator.h"
#include "policy.h"
#include "registry.h"
#include "udev_monitor.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t g_running = 1;

static void on_signal(int signo) {
  (void)signo;
  g_running = 0;
}

static void on_hotplug(usb_hotplug_action_t action, const usb_device_t* device, void* user) {
  usb_orchestrator_t* orch = user;
  if (action == USB_HOTPLUG_REMOVE) usb_orchestrator_on_removed(orch, device->device_id);
  else usb_orchestrator_on_added(orch, device);
}

static void on_device(const usb_device_t* device, const char* reason, void* user) {
  (void)reason;
  usb_dbus_emit_device(user, device);
}

static void on_session(const usb_session_t* session, void* user) { usb_dbus_emit_session(user, session); }

int main(int argc, char** argv) {
  int enable_dbus = 1;
  int auto_enum = 1;
  const char* allow_serial = NULL;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      fprintf(stderr, "Usage: %s [--no-dbus] [--no-auto-enum] [--allowlist SERIAL] [--debug]\n", argv[0]);
      return 0;
    }
    if (strcmp(argv[i], "--no-dbus") == 0) enable_dbus = 0;
    else if (strcmp(argv[i], "--no-auto-enum") == 0) auto_enum = 0;
    else if (strcmp(argv[i], "--debug") == 0) usb_log_set_level(USB_LOG_DEBUG);
    else if (strcmp(argv[i], "--allowlist") == 0 && i + 1 < argc) allow_serial = argv[++i];
    else {
      fprintf(stderr, "Unknown arg: %s\n", argv[i]);
      return 2;
    }
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  usb_registry_t* registry = usb_registry_create();
  usb_policy_t* policy = usb_policy_create();
  if (allow_serial) {
    usb_policy_set_allowlist(policy, 1);
    usb_policy_allow_serial(policy, allow_serial);
  }
  usb_orchestrator_t* orch = usb_orchestrator_create(registry, policy);
  usb_adapter_t aa;
  usb_adapter_t cp;
  usb_adapter_android_init(&aa);
  usb_adapter_carplay_init(&cp);
  usb_orchestrator_set_adapters(orch, &aa, &cp);

  usb_dbus_t* bus = NULL;
  if (enable_dbus) {
    bus = usb_dbus_start(registry, orch);
    usb_registry_set_callback(registry, on_device, bus);
    usb_orchestrator_set_session_callback(orch, on_session, bus);
  }

  usb_udev_monitor_t* mon = usb_udev_monitor_create();
  usb_udev_monitor_set_callback(mon, on_hotplug, orch);
  if (!usb_udev_monitor_start(mon)) {
    USB_LOG_ERROR("Failed to start UdevMonitor");
    usb_dbus_stop(bus);
    usb_udev_monitor_destroy(mon);
    usb_orchestrator_destroy(orch);
    usb_policy_destroy(policy);
    usb_registry_destroy(registry);
    return 1;
  }
  if (auto_enum) usb_udev_monitor_enumerate(mon);

  USB_LOG_INFO("usb-manager-c running (Host mode). Ctrl+C to stop.");
  while (g_running) usb_udev_monitor_poll(mon, 500);

  USB_LOG_INFO("Shutting down...");
  usb_dbus_stop(bus);
  usb_udev_monitor_destroy(mon);
  usb_orchestrator_destroy(orch);
  usb_policy_destroy(policy);
  usb_registry_destroy(registry);
  return 0;
}
