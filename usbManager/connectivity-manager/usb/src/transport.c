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

static int already_in_aoap(uint16_t vendor_id, uint16_t product_id)
{
    return vendor_id == 0x18d1 && product_id >= 0x2d00 && product_id <= 0x2d05;
}

static void detail_set(char* dst, size_t len, const char* text)
{
    snprintf(dst, len, "%s", text ? text : "");
}

void usb_probe_aoap(const usb_device_t* device, usb_aoap_result_t* out)
{
    libusb_context* ctx = NULL;
    libusb_device_handle* handle;
    uint16_t protocol = 0;
    int rc;

    if (!device || !out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->attempted = 1;
    if (already_in_aoap(device->vendor_id, device->product_id)) {
        out->supported = 1;
        detail_set(out->detail, sizeof(out->detail), "already_in_aoap_mode");
        return;
    }

    if (libusb_init(&ctx) != 0) {
        detail_set(out->detail, sizeof(out->detail), "libusb_init_failed");
        return;
    }
    handle = libusb_open_device_with_vid_pid(ctx, device->vendor_id, device->product_id);
    if (!handle) {
        detail_set(out->detail, sizeof(out->detail), "open_failed_need_permissions_or_device_busy");
        USB_LOG_WARN("AOAP probe: cannot open device");
        libusb_exit(ctx);
        return;
    }
    /* bmRequestType 0xC0: device-to-host, vendor, device. Request 51: GET_PROTOCOL. */
    rc = libusb_control_transfer(handle, 0xC0, USB_AOAP_GET_PROTOCOL, 0, 0,
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

void usb_probe_ncm(const usb_device_t* device, usb_ncm_result_t* out)
{
    DIR* dir;
    struct dirent* entry;

    if (!device || !out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    dir = opendir("/sys/class/net");
    if (!dir) {
        detail_set(out->detail, sizeof(out->detail), "no_sys_class_net");
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char driver_link[PATH_MAX];
        char driver_buf[PATH_MAX];
        char link_path[PATH_MAX];
        char link_buf[PATH_MAX];
        ssize_t driver_len;
        ssize_t link_len;
        int driver_match = 0;
        int path_match = 0;

        if (entry->d_name[0] == '.') {
            continue;
        }
        snprintf(driver_link, sizeof(driver_link), "/sys/class/net/%s/device/driver", entry->d_name);
        driver_len = readlink(driver_link, driver_buf, sizeof(driver_buf) - 1);
        if (driver_len > 0) {
            driver_buf[driver_len] = '\0';
            if (strstr(driver_buf, "cdc_ncm") || strstr(driver_buf, "rndis_host") ||
                strstr(driver_buf, "cdc_ether")) {
                driver_match = 1;
            }
        }
        snprintf(link_path, sizeof(link_path), "/sys/class/net/%s/device", entry->d_name);
        link_len = readlink(link_path, link_buf, sizeof(link_buf) - 1);
        if (link_len > 0 && device->sys_path[0]) {
            char absolute[PATH_MAX];
            char resolved[PATH_MAX];
            const char* path;

            link_buf[link_len] = '\0';
            if (link_buf[0] != '/') {
                snprintf(absolute, sizeof(absolute), "/sys/class/net/%.255s/%.3500s", entry->d_name, link_buf);
            } else {
                snprintf(absolute, sizeof(absolute), "%.4095s", link_buf);
            }
            path = realpath(absolute, resolved) ? resolved : absolute;
            if (strstr(path, device->sys_path)) {
                path_match = 1;
            }
        }
        /*
         * Ưu tiên iface đúng cây sysfs của điện thoại này.
         * Driver NCM/RNDIS chỉ nhận khi máy đã được phân loại là điện thoại,
         * để không gán nhầm usb0 của máy khác.
         */
        if (path_match || (driver_match &&
                           (device->type == USB_TYPE_ANDROID || device->type == USB_TYPE_IPHONE))) {
            out->iface_present = 1;
            snprintf(out->if_name, sizeof(out->if_name), "%.31s", entry->d_name);
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
