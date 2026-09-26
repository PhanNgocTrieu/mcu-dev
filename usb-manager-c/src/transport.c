#include "transport.h"

#include "log.h"

#include <libusb-1.0/libusb.h>

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USB_AOAP_GET_PROTOCOL 51

static int already_in_aoap(uint16_t vid, uint16_t pid) {
  return vid == 0x18d1 && pid >= 0x2d00 && pid <= 0x2d05;
}

static void detail_set(char* dst, size_t n, const char* s) { snprintf(dst, n, "%s", s ? s : ""); }

void usb_probe_aoap(const usb_device_t* device, usb_aoap_result_t* out) {
  if (!device || !out) return;
  memset(out, 0, sizeof(*out));
  out->attempted = 1;
  if (already_in_aoap(device->vendor_id, device->product_id)) {
    out->supported = 1;
    detail_set(out->detail, sizeof(out->detail), "already_in_aoap_mode");
    return;
  }

  libusb_context* ctx = NULL;
  if (libusb_init(&ctx) != 0) {
    detail_set(out->detail, sizeof(out->detail), "libusb_init_failed");
    return;
  }
  libusb_device_handle* handle =
      libusb_open_device_with_vid_pid(ctx, device->vendor_id, device->product_id);
  if (!handle) {
    detail_set(out->detail, sizeof(out->detail), "open_failed_need_permissions_or_device_busy");
    USB_LOG_WARN("AOAP probe: cannot open device");
    libusb_exit(ctx);
    return;
  }
  uint16_t protocol = 0;
  int rc = libusb_control_transfer(handle, 0xC0, USB_AOAP_GET_PROTOCOL, 0, 0,
                                   (unsigned char*)&protocol, sizeof(protocol), 1000);
  if (rc == (int)sizeof(protocol) && protocol >= 1) {
    out->supported = 1;
    snprintf(out->detail, sizeof(out->detail), "aoap_protocol=%u", protocol);
    USB_LOG_INFO(out->detail);
  } else {
    snprintf(out->detail, sizeof(out->detail), "get_protocol_failed_rc=%d", rc);
    USB_LOG_INFO(out->detail);
  }
  libusb_close(handle);
  libusb_exit(ctx);
}

void usb_probe_ncm(const usb_device_t* device, usb_ncm_result_t* out) {
  if (!device || !out) return;
  memset(out, 0, sizeof(*out));
  DIR* dir = opendir("/sys/class/net");
  if (!dir) {
    detail_set(out->detail, sizeof(out->detail), "no_sys_class_net");
    return;
  }
  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.') continue;
    char driver_link[PATH_MAX];
    snprintf(driver_link, sizeof(driver_link), "/sys/class/net/%s/device/driver", ent->d_name);
    char dbuf[PATH_MAX];
    ssize_t dn = readlink(driver_link, dbuf, sizeof(dbuf) - 1);
    int driver_match = 0;
    if (dn > 0) {
      dbuf[dn] = '\0';
      if (strstr(dbuf, "cdc_ncm") || strstr(dbuf, "rndis_host") || strstr(dbuf, "cdc_ether")) {
        driver_match = 1;
      }
    }
    char link_path[PATH_MAX];
    snprintf(link_path, sizeof(link_path), "/sys/class/net/%s/device", ent->d_name);
    char buf[PATH_MAX];
    ssize_t n = readlink(link_path, buf, sizeof(buf) - 1);
    int path_match = 0;
    if (n > 0 && device->sys_path[0]) {
      buf[n] = '\0';
      char abs[PATH_MAX];
      if (buf[0] != '/') snprintf(abs, sizeof(abs), "/sys/class/net/%s/%s", ent->d_name, buf);
      else snprintf(abs, sizeof(abs), "%s", buf);
      char real[PATH_MAX];
      const char* resolved = abs;
      if (realpath(abs, real)) resolved = real;
      if (strstr(resolved, device->sys_path)) path_match = 1;
    }
    int accept = path_match || (driver_match && (device->type == USB_TYPE_ANDROID ||
                                                 device->type == USB_TYPE_IPHONE));
    if (accept) {
      out->iface_present = 1;
      snprintf(out->if_name, sizeof(out->if_name), "%s", ent->d_name);
      detail_set(out->detail, sizeof(out->detail), path_match ? "matched_syspath" : "matched_driver");
      USB_LOG_INFO(out->if_name);
      break;
    }
  }
  closedir(dir);
  if (!out->iface_present && !out->detail[0]) {
    detail_set(out->detail, sizeof(out->detail), "no_ncm_rndis_iface");
  }
}
