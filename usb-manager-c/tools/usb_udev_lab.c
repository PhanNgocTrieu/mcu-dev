#include "log.h"
#include "udev_monitor.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t g_run = 1;

static void on_sig(int signo) {
  (void)signo;
  g_run = 0;
}

static void on_event(usb_hotplug_action_t action, const usb_device_t* d, void* user) {
  (void)user;
  const char* act = action == USB_HOTPLUG_REMOVE ? "REMOVE" : action == USB_HOTPLUG_CHANGE ? "CHANGE" : "ADD";
  printf("%-6s id=%-16s type=%-12s %04x:%04x mfr=\"%s\" product=\"%s\" serial=\"%s\" ifaces=\"%s\"\n", act,
         d->device_id, usb_type_str(d->type), d->vendor_id, d->product_id, d->manufacturer, d->product, d->serial,
         d->interfaces);
}

int main(int argc, char** argv) {
  int once = 0;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--once") == 0) once = 1;
  }
  signal(SIGINT, on_sig);
  usb_udev_monitor_t* mon = usb_udev_monitor_create();
  usb_udev_monitor_set_callback(mon, on_event, NULL);
  if (!usb_udev_monitor_start(mon)) {
    fprintf(stderr, "Failed to start udev monitor\n");
    usb_udev_monitor_destroy(mon);
    return 1;
  }
  usb_udev_monitor_enumerate(mon);
  if (once) {
    usb_udev_monitor_destroy(mon);
    return 0;
  }
  fprintf(stderr, "Listening for USB plug/unplug... Ctrl+C to quit\n");
  while (g_run) usb_udev_monitor_poll(mon, 500);
  usb_udev_monitor_destroy(mon);
  return 0;
}
