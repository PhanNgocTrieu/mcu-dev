/**
 * @file enum.c
 * @brief Đọc /sys/bus/usb/devices và gom thông tin mỗi USB device.
 *
 * Chỉ lấy node có idVendor (bỏ hub/interface thuần). Gắn cờ class
 * (NCM/ECM/RNDIS/ADB/...) từ bInterfaceClass + tên driver bound.
 * Bỏ qua root hub Linux (vid 0x1d6b).
 */
#include "usbdrv.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_text(const char *path, char *out, size_t n)
{
    FILE *fp;
    size_t len;

    if (!out || n == 0) {
        return -1;
    }
    out[0] = '\0';
    fp = fopen(path, "re");
    if (!fp) {
        return -1;
    }
    if (!fgets(out, (int)n, fp)) {
        fclose(fp);
        out[0] = '\0';
        return -1;
    }
    fclose(fp);
    len = strlen(out);
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
        out[--len] = '\0';
    }
    return 0;
}

static int read_hex16(const char *path, uint16_t *out)
{
    char tmp[32];
    char *end = NULL;
    unsigned long v;

    if (read_text(path, tmp, sizeof tmp) != 0) {
        return -1;
    }
    v = strtoul(tmp, &end, 16);
    if (!end || *end) {
        return -1;
    }
    *out = (uint16_t)v;
    return 0;
}

static int read_dec(const char *path, int *out)
{
    char tmp[32];
    char *end = NULL;

    if (read_text(path, tmp, sizeof tmp) != 0) {
        return -1;
    }
    *out = (int)strtol(tmp, &end, 10);
    if (!end || *end) {
        return -1;
    }
    return 0;
}

static void append_token(char *dst, size_t n, const char *token)
{
    size_t len = strlen(dst);
    size_t tlen = strlen(token);

    if (len && len + 1 < n) {
        dst[len++] = ',';
        dst[len] = '\0';
    }
    if (len + tlen >= n) {
        return;
    }
    memcpy(dst + len, token, tlen + 1);
}

static int has_driver_name(const char *list, const char *name)
{
    char tmp[USBDRV_LIST_LEN];
    char *save = NULL;
    char *tok;

    snprintf(tmp, sizeof tmp, "%s", list);
    for (tok = strtok_r(tmp, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        if (strcmp(tok, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Ghi nhận một interface: cập nhật danh sách + cờ class cho classify phía usb-man. */
static void note_iface(usbdrv_device_t *dev, unsigned cls, unsigned sub, unsigned proto,
                       const char *driver)
{
    char triple[16];

    snprintf(triple, sizeof triple, "%02x/%02x/%02x", cls, sub, proto);
    append_token(dev->interfaces, sizeof dev->interfaces, triple);
    if (driver && driver[0] && !has_driver_name(dev->drivers, driver)) {
        append_token(dev->drivers, sizeof dev->drivers, driver);
    }
    if (cls == 0x02 && sub == 0x0d) {
        dev->class_ncm = 1; /* CDC-NCM — bắt buộc cho CarPlay path */
    }
    if (cls == 0x02 && sub == 0x06) {
        dev->class_ecm = 1; /* CDC-ECM */
    }
    if (cls == 0x0a || (cls == 0x02 && sub == 0x02)) {
        /* CDC data / ACM thôi chưa đủ để gọi là NCM. */
    }
    if ((cls == 0xe0 && sub == 0x01 && proto == 0x03) || (driver && strcmp(driver, "rndis_host") == 0)) {
        dev->class_rndis = 1;
    }
    if (driver && strcmp(driver, "ipheth") == 0) {
        dev->class_ipheth = 1; /* Apple tether cũ — policy HUPI từ chối */
    }
    if (cls == 0x08 || (driver && strcmp(driver, "usb-storage") == 0)) {
        dev->class_storage = 1;
    }
    if (cls == 0x03 || (driver && strcmp(driver, "usbhid") == 0)) {
        dev->class_hid = 1;
    }
    if (cls == 0xff && sub == 0x42 && proto == 0x01) {
        dev->class_adb = 1; /* Android Debug Bridge */
    }
    if (driver && strcmp(driver, "cdc_ncm") == 0) {
        dev->class_ncm = 1;
    }
    if (driver && strcmp(driver, "cdc_ether") == 0) {
        dev->class_ecm = 1;
    }
}

static void read_driver_name(const char *iface_path, char *out, size_t n)
{
    char link[USBDRV_PATH_LEN];
    char path[USBDRV_PATH_LEN];
    ssize_t got;
    const char *base;

    out[0] = '\0';
    snprintf(path, sizeof path, "%s/driver", iface_path);
    got = readlink(path, link, sizeof link - 1);
    if (got < 0) {
        return;
    }
    link[got] = '\0';
    base = strrchr(link, '/');
    snprintf(out, n, "%s", base ? base + 1 : link);
}

static void find_net(const char *iface_path, char *out, size_t n)
{
    char netdir[USBDRV_PATH_LEN];
    DIR *dir;
    struct dirent *de;

    if (out[0]) {
        return;
    }
    snprintf(netdir, sizeof netdir, "%s/net", iface_path);
    dir = opendir(netdir);
    if (!dir) {
        return;
    }
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') {
            continue;
        }
        snprintf(out, n, "%s", de->d_name);
        break;
    }
    closedir(dir);
}

/* Đọc một device node sysfs (có idVendor) + duyệt các interface "bus-port:cfg.if". */
static void read_device(const char *root, const char *name, usbdrv_device_t *dev)
{
    char path[USBDRV_PATH_LEN];
    DIR *dir;
    struct dirent *de;

    memset(dev, 0, sizeof *dev);
    snprintf(dev->sys_name, sizeof dev->sys_name, "%s", name);
    snprintf(dev->sys_path, sizeof dev->sys_path, "%s/%s", root, name);
    snprintf(path, sizeof path, "%s/idVendor", dev->sys_path);
    read_hex16(path, &dev->vendor_id);
    snprintf(path, sizeof path, "%s/idProduct", dev->sys_path);
    read_hex16(path, &dev->product_id);
    snprintf(path, sizeof path, "%s/manufacturer", dev->sys_path);
    read_text(path, dev->manufacturer, sizeof dev->manufacturer);
    snprintf(path, sizeof path, "%s/product", dev->sys_path);
    read_text(path, dev->product, sizeof dev->product);
    snprintf(path, sizeof path, "%s/serial", dev->sys_path);
    read_text(path, dev->serial, sizeof dev->serial);
    snprintf(path, sizeof path, "%s/busnum", dev->sys_path);
    read_dec(path, &dev->busnum);
    snprintf(path, sizeof path, "%s/devnum", dev->sys_path);
    read_dec(path, &dev->devnum);
    /* usbfs path chuẩn theo busnum/devnum — dùng cho USBDEVFS_CONTROL. */
    if (dev->busnum > 0 && dev->devnum > 0) {
        snprintf(dev->devnode, sizeof dev->devnode, "/dev/bus/usb/%03d/%03d", dev->busnum, dev->devnum);
    }
    /* Google AOAP đã vào accessory mode (trước/sau AOA START). */
    if (dev->vendor_id == 0x18d1 && (dev->product_id == 0x2d00 || dev->product_id == 0x2d01)) {
        dev->class_accessory = 1;
    }

    dir = opendir(dev->sys_path);
    if (!dir) {
        return;
    }
    while ((de = readdir(dir)) != NULL) {
        unsigned cls = 0, sub = 0, proto = 0;
        char iface[USBDRV_PATH_LEN];
        char driver[USBDRV_NAME_LEN];
        uint16_t v;

        /* Interface sysfs có dạng "1-1:1.0" — có dấu ':'. Bỏ qua file khác. */
        if (!strchr(de->d_name, ':')) {
            continue;
        }
        snprintf(iface, sizeof iface, "%s/%s", dev->sys_path, de->d_name);
        snprintf(path, sizeof path, "%s/bInterfaceClass", iface);
        if (read_hex16(path, &v) == 0) {
            cls = v;
        }
        snprintf(path, sizeof path, "%s/bInterfaceSubClass", iface);
        if (read_hex16(path, &v) == 0) {
            sub = v;
        }
        snprintf(path, sizeof path, "%s/bInterfaceProtocol", iface);
        if (read_hex16(path, &v) == 0) {
            proto = v;
        }
        read_driver_name(iface, driver, sizeof driver); /* symlink .../driver → tên module */
        note_iface(dev, cls, sub, proto, driver);
        find_net(iface, dev->net_iface, sizeof dev->net_iface); /* net/usb0 nếu CDC */
    }
    closedir(dir);
}

int usbdrv_enum_root(const char *sysfs_devices, usbdrv_device_t *out, size_t capacity, size_t *count)
{
    DIR *dir;
    struct dirent *de;

    if (!count) {
        return -1;
    }
    *count = 0;
    dir = opendir(sysfs_devices);
    if (!dir) {
        return -1;
    }
    while ((de = readdir(dir)) != NULL) {
        char vendor[USBDRV_PATH_LEN];
        if (de->d_name[0] == '.') {
            continue;
        }
        /* Có idVendor → đây là USB device (không phải interface/hub port trống). */
        snprintf(vendor, sizeof vendor, "%s/%s/idVendor", sysfs_devices, de->d_name);
        if (access(vendor, R_OK) != 0) {
            continue;
        }
        if (*count >= capacity) {
            break;
        }
        read_device(sysfs_devices, de->d_name, &out[*count]);
        if (out[*count].vendor_id == 0x1d6b) {
            continue; /* Linux root hub — bỏ qua */
        }
        (*count)++;
    }
    closedir(dir);
    return 0;
}
