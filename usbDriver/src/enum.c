#include "usbdrv.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void copy_str(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (cap == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int read_text(const char *path, char *dst, size_t cap)
{
    FILE *file;
    if (cap == 0) {
        return -1;
    }
    dst[0] = '\0';
    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }
    if (fgets(dst, (int)cap, file) == NULL) {
        dst[0] = '\0';
        fclose(file);
        return -1;
    }
    fclose(file);
    size_t n = strlen(dst);
    while (n > 0 && (dst[n - 1] == '\n' || dst[n - 1] == '\r')) {
        dst[--n] = '\0';
    }
    return 0;
}

static int join_path(char *dst, size_t cap, const char *a, const char *b)
{
    int n = snprintf(dst, cap, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= cap) {
        if (cap > 0) {
            dst[0] = '\0';
        }
        return -1;
    }
    return 0;
}

static int has_token(const char *list, const char *token)
{
    size_t n = strlen(token);
    const char *p = list;
    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        if (strncmp(p, token, n) == 0 && (p[n] == '\0' || p[n] == ' ')) {
            return 1;
        }
        while (*p != '\0' && *p != ' ') {
            p++;
        }
    }
    return 0;
}

static void append_token(char *dst, size_t cap, const char *token)
{
    size_t len;
    size_t add;
    int need_space;
    if (token == NULL || token[0] == '\0' || cap == 0 || has_token(dst, token)) {
        return;
    }
    len = strlen(dst);
    add = strlen(token);
    need_space = len > 0;
    if (len + (size_t)need_space + add + 1 > cap) {
        return;
    }
    if (need_space) {
        dst[len++] = ' ';
    }
    memcpy(dst + len, token, add + 1);
}

static int is_device_name(const char *name)
{
    const char *p;
    if (name == NULL || !isdigit((unsigned char)name[0])) {
        return 0;
    }
    if (strchr(name, ':') != NULL || strchr(name, '-') == NULL) {
        return 0;
    }
    for (p = name; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p) && *p != '-' && *p != '.') {
            return 0;
        }
    }
    return 1;
}

static int parse_u16_file(const char *path, uint16_t *out, int base)
{
    char text[32];
    char *end = NULL;
    unsigned long value;
    if (read_text(path, text, sizeof text) != 0) {
        return -1;
    }
    value = strtoul(text, &end, base);
    if (end == text) {
        return -1;
    }
    *out = (uint16_t)value;
    return 0;
}

static int parse_int_file(const char *path, int *out)
{
    char text[32];
    if (read_text(path, text, sizeof text) != 0) {
        return -1;
    }
    *out = atoi(text);
    return 0;
}

static void note_interface(usbdrv_device_t *dev, unsigned cls, unsigned sub, unsigned proto,
                           const char *driver)
{
    char token[16];
    snprintf(token, sizeof token, "%02x/%02x/%02x", cls & 0xffu, sub & 0xffu, proto & 0xffu);
    append_token(dev->interfaces, sizeof dev->interfaces, token);
    if (driver != NULL && driver[0] != '\0') {
        append_token(dev->drivers, sizeof dev->drivers, driver);
    }
    if (cls == 0x02 && sub == 0x0d) {
        dev->class_ncm = 1;
    }
    if (cls == 0x02 && sub == 0x06) {
        dev->class_ecm = 1;
    }
    if ((cls == 0x02 && sub == 0x02 && proto == 0xff) || (cls == 0xe0 && sub == 0x01 && proto == 0x03)) {
        dev->class_rndis = 1;
    }
    if (cls == 0x02 && sub == 0x02 && proto == 0x01) {
        dev->class_acm = 1;
    }
    if (cls == 0x08) {
        dev->class_storage = 1;
    }
    if (cls == 0x03) {
        dev->class_hid = 1;
    }
    if (cls == 0xff && sub == 0x42) {
        dev->class_adb = 1;
    }
    if (driver != NULL) {
        if (strcmp(driver, "cdc_ncm") == 0) {
            dev->class_ncm = 1;
        } else if (strcmp(driver, "cdc_ether") == 0) {
            dev->class_ecm = 1;
        } else if (strcmp(driver, "rndis_host") == 0) {
            dev->class_rndis = 1;
        } else if (strcmp(driver, "ipheth") == 0) {
            dev->class_ipheth = 1;
        } else if (strcmp(driver, "usb-storage") == 0 || strcmp(driver, "uas") == 0) {
            dev->class_storage = 1;
        } else if (strcmp(driver, "usbhid") == 0) {
            dev->class_hid = 1;
        } else if (strcmp(driver, "cdc_acm") == 0) {
            dev->class_acm = 1;
        }
    }
}

static void read_driver_name(const char *iface_path, char *name, size_t cap)
{
    char link_path[USBDRV_PATH_LEN];
    char target[USBDRV_PATH_LEN];
    ssize_t n;
    const char *base;
    name[0] = '\0';
    if (join_path(link_path, sizeof link_path, iface_path, "driver") != 0) {
        return;
    }
    n = readlink(link_path, target, sizeof target - 1);
    if (n < 0) {
        return;
    }
    target[n] = '\0';
    base = strrchr(target, '/');
    copy_str(name, cap, base != NULL ? base + 1 : target);
}

static void find_net_iface(const char *iface_path, char *out, size_t cap)
{
    char net_path[USBDRV_PATH_LEN];
    DIR *dir;
    struct dirent *ent;
    if (out[0] != '\0') {
        return;
    }
    if (join_path(net_path, sizeof net_path, iface_path, "net") != 0) {
        return;
    }
    dir = opendir(net_path);
    if (dir == NULL) {
        return;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        copy_str(out, cap, ent->d_name);
        break;
    }
    closedir(dir);
}

static void find_block(const char *dir_path, int depth, char *out, size_t cap)
{
    DIR *dir;
    struct dirent *ent;
    if (depth > 6 || out[0] != '\0') {
        return;
    }
    dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }
    while ((ent = readdir(dir)) != NULL && out[0] == '\0') {
        char child[USBDRV_PATH_LEN];
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (join_path(child, sizeof child, dir_path, ent->d_name) != 0) {
            continue;
        }
        if (strcmp(ent->d_name, "block") == 0) {
            DIR *block = opendir(child);
            if (block != NULL) {
                struct dirent *dev;
                while ((dev = readdir(block)) != NULL) {
                    if (dev->d_name[0] == '.') {
                        continue;
                    }
                    copy_str(out, cap, dev->d_name);
                    break;
                }
                closedir(block);
            }
            continue;
        }
        find_block(child, depth + 1, out, cap);
    }
    closedir(dir);
}

static void note_aoap(usbdrv_device_t *dev)
{
    if (dev->vendor_id == 0x18d1 && dev->product_id >= 0x2d00 && dev->product_id <= 0x2d05) {
        dev->class_aoap = 1;
    }
}

static void read_device(const char *root, const char *name, usbdrv_device_t *dev)
{
    char path[USBDRV_PATH_LEN];
    DIR *dir;
    struct dirent *ent;
    memset(dev, 0, sizeof *dev);
    copy_str(dev->sys_name, sizeof dev->sys_name, name);
    if (join_path(dev->sys_path, sizeof dev->sys_path, root, name) != 0) {
        return;
    }
    join_path(path, sizeof path, dev->sys_path, "idVendor");
    parse_u16_file(path, &dev->vendor_id, 16);
    join_path(path, sizeof path, dev->sys_path, "idProduct");
    parse_u16_file(path, &dev->product_id, 16);
    join_path(path, sizeof path, dev->sys_path, "manufacturer");
    read_text(path, dev->manufacturer, sizeof dev->manufacturer);
    join_path(path, sizeof path, dev->sys_path, "product");
    read_text(path, dev->product, sizeof dev->product);
    join_path(path, sizeof path, dev->sys_path, "serial");
    read_text(path, dev->serial, sizeof dev->serial);
    join_path(path, sizeof path, dev->sys_path, "busnum");
    parse_int_file(path, &dev->busnum);
    join_path(path, sizeof path, dev->sys_path, "devnum");
    parse_int_file(path, &dev->devnum);
    if (dev->busnum > 0 && dev->devnum > 0) {
        snprintf(dev->devnode, sizeof dev->devnode, "/dev/bus/usb/%03d/%03d", dev->busnum, dev->devnum);
    }
    note_aoap(dev);

    dir = opendir(dev->sys_path);
    if (dir == NULL) {
        return;
    }
    while ((ent = readdir(dir)) != NULL) {
        char iface[USBDRV_PATH_LEN];
        char cls_path[USBDRV_PATH_LEN];
        char driver[USBDRV_NAME_LEN];
        uint16_t cls = 0;
        uint16_t sub = 0;
        uint16_t proto = 0;
        size_t name_len = strlen(name);
        if (strncmp(ent->d_name, name, name_len) != 0 || ent->d_name[name_len] != ':') {
            continue;
        }
        if (join_path(iface, sizeof iface, dev->sys_path, ent->d_name) != 0) {
            continue;
        }
        join_path(cls_path, sizeof cls_path, iface, "bInterfaceClass");
        parse_u16_file(cls_path, &cls, 16);
        join_path(cls_path, sizeof cls_path, iface, "bInterfaceSubClass");
        parse_u16_file(cls_path, &sub, 16);
        join_path(cls_path, sizeof cls_path, iface, "bInterfaceProtocol");
        parse_u16_file(cls_path, &proto, 16);
        read_driver_name(iface, driver, sizeof driver);
        note_interface(dev, cls, sub, proto, driver);
        find_net_iface(iface, dev->net_iface, sizeof dev->net_iface);
        if (dev->block_dev[0] == '\0') {
            find_block(iface, 0, dev->block_dev, sizeof dev->block_dev);
        }
    }
    closedir(dir);
}

int usbdrv_enum_root(const char *sysfs_devices, usbdrv_device_t *out, size_t capacity, size_t *count)
{
    DIR *dir;
    struct dirent *ent;
    size_t stored = 0;
    if (sysfs_devices == NULL || count == NULL || (capacity > 0 && out == NULL)) {
        return -1;
    }
    *count = 0;
    dir = opendir(sysfs_devices);
    if (dir == NULL) {
        return -1;
    }
    while ((ent = readdir(dir)) != NULL) {
        usbdrv_device_t device;
        if (!is_device_name(ent->d_name)) {
            continue;
        }
        if (stored >= capacity) {
            continue;
        }
        read_device(sysfs_devices, ent->d_name, &device);
        if (device.vendor_id == 0 && device.product_id == 0) {
            continue;
        }
        out[stored++] = device;
    }
    closedir(dir);
    *count = stored;
    return 0;
}
