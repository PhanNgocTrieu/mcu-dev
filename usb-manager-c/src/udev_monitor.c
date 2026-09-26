#include "udev_monitor.h"

#include "classifier.h"
#include "log.h"

#include <libudev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

struct usb_udev_monitor {
  struct udev* udev;
  struct udev_monitor* mon;
  usb_hotplug_cb cb;
  void* user;
  int running;
};

static uint16_t parse_hex(const char* s) {
  unsigned v = 0;
  if (!s || !*s) return 0;
  sscanf(s, "%x", &v);
  return (uint16_t)v;
}

static void copy_str(char* dst, size_t n, const char* src) {
  if (!dst || n == 0) return;
  if (!src) src = "";
  snprintf(dst, n, "%s", src);
}

static void fill_device(struct udev_device* dev, usb_device_t* info) {
  memset(info, 0, sizeof(*info));
  const char* syspath = udev_device_get_syspath(dev);
  const char* bus = udev_device_get_sysattr_value(dev, "busnum");
  const char* addr = udev_device_get_sysattr_value(dev, "devnum");
  const char* node = udev_device_get_devnode(dev);
  copy_str(info->sys_path, sizeof(info->sys_path), syspath);
  copy_str(info->dev_node, sizeof(info->dev_node), node);
  if (bus && addr) {
    snprintf(info->device_id, sizeof(info->device_id), "usb-%s-%s", bus, addr);
  } else if (syspath) {
    snprintf(info->device_id, sizeof(info->device_id), "sys:%s", syspath);
  } else {
    copy_str(info->device_id, sizeof(info->device_id), "usb-unknown");
  }

  const char* vid = udev_device_get_property_value(dev, "ID_VENDOR_ID");
  const char* pid = udev_device_get_property_value(dev, "ID_MODEL_ID");
  if (!vid) vid = udev_device_get_sysattr_value(dev, "idVendor");
  if (!pid) pid = udev_device_get_sysattr_value(dev, "idProduct");
  info->vendor_id = parse_hex(vid);
  info->product_id = parse_hex(pid);

  const char* mfr = udev_device_get_property_value(dev, "ID_VENDOR");
  if (!mfr) mfr = udev_device_get_sysattr_value(dev, "manufacturer");
  const char* prod = udev_device_get_property_value(dev, "ID_MODEL");
  if (!prod) prod = udev_device_get_sysattr_value(dev, "product");
  const char* serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
  if (!serial) serial = udev_device_get_sysattr_value(dev, "serial");
  const char* ifaces = udev_device_get_property_value(dev, "ID_USB_INTERFACES");
  copy_str(info->manufacturer, sizeof(info->manufacturer), mfr);
  copy_str(info->product, sizeof(info->product), prod);
  copy_str(info->serial, sizeof(info->serial), serial);
  copy_str(info->interfaces, sizeof(info->interfaces), ifaces);
  info->state = USB_STATE_ENUMERATING;
  usb_classify_enrich(info);
}

static int is_usb_device(struct udev_device* dev) {
  const char* subsystem = udev_device_get_subsystem(dev);
  const char* devtype = udev_device_get_devtype(dev);
  return subsystem && strcmp(subsystem, "usb") == 0 && devtype && strcmp(devtype, "usb_device") == 0;
}

usb_udev_monitor_t* usb_udev_monitor_create(void) { return calloc(1, sizeof(usb_udev_monitor_t)); }

void usb_udev_monitor_destroy(usb_udev_monitor_t* mon) {
  if (!mon) return;
  if (mon->mon) udev_monitor_unref(mon->mon);
  if (mon->udev) udev_unref(mon->udev);
  free(mon);
}

void usb_udev_monitor_set_callback(usb_udev_monitor_t* mon, usb_hotplug_cb cb, void* user) {
  if (!mon) return;
  mon->cb = cb;
  mon->user = user;
}

int usb_udev_monitor_start(usb_udev_monitor_t* mon) {
  if (!mon) return 0;
  if (mon->running) return 1;
  mon->udev = udev_new();
  if (!mon->udev) {
    USB_LOG_ERROR("udev_new failed");
    return 0;
  }
  mon->mon = udev_monitor_new_from_netlink(mon->udev, "udev");
  if (!mon->mon) {
    USB_LOG_ERROR("udev_monitor_new_from_netlink failed");
    udev_unref(mon->udev);
    mon->udev = NULL;
    return 0;
  }
  udev_monitor_filter_add_match_subsystem_devtype(mon->mon, "usb", "usb_device");
  udev_monitor_enable_receiving(mon->mon);
  mon->running = 1;
  USB_LOG_INFO("UdevMonitor started");
  return 1;
}

int usb_udev_monitor_poll(usb_udev_monitor_t* mon, int timeout_ms) {
  if (!mon || !mon->mon || !mon->cb) return 0;
  int fd = udev_monitor_get_fd(mon->mon);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  struct timeval* ptv = timeout_ms < 0 ? NULL : &tv;
  int ret = select(fd + 1, &fds, NULL, NULL, ptv);
  if (ret <= 0 || !FD_ISSET(fd, &fds)) return 0;

  struct udev_device* dev = udev_monitor_receive_device(mon->mon);
  if (!dev) return 0;
  if (!is_usb_device(dev)) {
    udev_device_unref(dev);
    return 0;
  }
  const char* action = udev_device_get_action(dev);
  usb_hotplug_action_t kind = USB_HOTPLUG_ADD;
  if (action && strcmp(action, "remove") == 0) kind = USB_HOTPLUG_REMOVE;
  else if (action && strcmp(action, "change") == 0) kind = USB_HOTPLUG_CHANGE;

  usb_device_t info;
  fill_device(dev, &info);
  char msg[256];
  snprintf(msg, sizeof(msg), "hotplug %s %s %s vid=%u pid=%u",
           kind == USB_HOTPLUG_REMOVE ? "remove" : "add/change", info.device_id, usb_type_str(info.type),
           info.vendor_id, info.product_id);
  USB_LOG_INFO(msg);
  mon->cb(kind, &info, mon->user);
  udev_device_unref(dev);
  return 1;
}

void usb_udev_monitor_enumerate(usb_udev_monitor_t* mon) {
  if (!mon || !mon->udev || !mon->cb) return;
  struct udev_enumerate* enumerate = udev_enumerate_new(mon->udev);
  if (!enumerate) return;
  udev_enumerate_add_match_subsystem(enumerate, "usb");
  udev_enumerate_scan_devices(enumerate);
  struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry* entry = NULL;
  udev_list_entry_foreach(entry, devices) {
    const char* path = udev_list_entry_get_name(entry);
    struct udev_device* dev = udev_device_new_from_syspath(mon->udev, path);
    if (!dev) continue;
    if (is_usb_device(dev)) {
      usb_device_t info;
      fill_device(dev, &info);
      mon->cb(USB_HOTPLUG_ADD, &info, mon->user);
    }
    udev_device_unref(dev);
  }
  udev_enumerate_unref(enumerate);
}
