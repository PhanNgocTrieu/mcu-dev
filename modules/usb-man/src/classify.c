/**
 * @file classify.c
 * @brief Phân loại thiết bị USB → kind dùng cho session policy.
 *
 * Ưu tiên (cao → thấp):
 *   AOAP (đã accessory) → Apple/ipheth|NCM|wait → Android ADB → storage → HID
 *
 * Cũng parse dòng `dev ...` từ usb-driverd thành usbman_dev_t.
 */
#include "usbman.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>

const char *usbman_kind_str(usbman_kind_t kind)
{
    switch (kind) {
    case USBMAN_KIND_ANDROID:
        return "android";
    case USBMAN_KIND_ANDROID_AOAP:
        return "android-aoap";
    case USBMAN_KIND_CARPLAY:
        return "carplay";
    case USBMAN_KIND_APPLE_WAIT:
        return "apple-wait";
    case USBMAN_KIND_APPLE_IPHETH:
        return "apple-ipheth";
    case USBMAN_KIND_STORAGE:
        return "storage";
    case USBMAN_KIND_HID:
        return "hid";
    default:
        return "other";
    }
}

usbman_kind_t usbman_classify(const usbman_dev_t *dev)
{
    if (!dev) {
        return USBMAN_KIND_OTHER;
    }
    /* Google AOAP: pid 0x2d00 (AOA) / 0x2d01 (AOA + ADB) */
    if (dev->accessory || (dev->vid == 0x18d1 && (dev->pid == 0x2d00 || dev->pid == 0x2d01))) {
        return USBMAN_KIND_ANDROID_AOAP;
    }
    if (dev->apple || dev->vid == 0x05ac) {
        if (dev->ipheth && !dev->ncm) {
            return USBMAN_KIND_APPLE_IPHETH; /* bị policy từ chối */
        }
        if (dev->ncm) {
            return USBMAN_KIND_CARPLAY; /* đủ điều kiện projection */
        }
        return USBMAN_KIND_APPLE_WAIT; /* chờ interface NCM */
    }
    if (dev->adb || dev->vid == 0x18d1) {
        return USBMAN_KIND_ANDROID; /* cần AOA switch trước khi media */
    }
    if (dev->storage) {
        return USBMAN_KIND_STORAGE;
    }
    if (dev->hid) {
        return USBMAN_KIND_HID;
    }
    return USBMAN_KIND_OTHER;
}

/* Lấy key=value rồi unescape (đảo của hupi_escape phía driver). */
static void take(const char *line, const char *key, char *dst, size_t n)
{
    char raw[192];
    if (hupi_kv_get(line, key, raw, sizeof raw) != 0) {
        dst[0] = '\0';
        return;
    }
    hupi_unescape(raw, dst, n);
}

/* Parse "dev id=... vid=..." → struct. Bỏ qua "dev gone ...". */
int usbman_parse_dev(const char *line, usbman_dev_t *dev)
{
    if (!line || strncmp(line, "dev ", 4) != 0 || !dev) {
        return -1;
    }
    if (strncmp(line, "dev gone ", 9) == 0) {
        return -1;
    }
    memset(dev, 0, sizeof *dev);
    take(line, "id", dev->id, sizeof dev->id);
    take(line, "serial", dev->serial, sizeof dev->serial);
    take(line, "mfg", dev->mfg, sizeof dev->mfg);
    take(line, "prod", dev->prod, sizeof dev->prod);
    take(line, "ifaces", dev->ifaces, sizeof dev->ifaces);
    take(line, "drivers", dev->drivers, sizeof dev->drivers);
    take(line, "net", dev->net, sizeof dev->net);
    take(line, "node", dev->node, sizeof dev->node);
    if (dev->id[0] == '\0') {
        return -1;
    }
    hupi_kv_get_uint(line, "vid", &dev->vid);
    hupi_kv_get_uint(line, "pid", &dev->pid);
    hupi_kv_get_int(line, "ncm", &dev->ncm);
    hupi_kv_get_int(line, "ecm", &dev->ecm);
    hupi_kv_get_int(line, "rndis", &dev->rndis);
    hupi_kv_get_int(line, "ipheth", &dev->ipheth);
    hupi_kv_get_int(line, "storage", &dev->storage);
    hupi_kv_get_int(line, "hid", &dev->hid);
    hupi_kv_get_int(line, "adb", &dev->adb);
    hupi_kv_get_int(line, "accessory", &dev->accessory);
    hupi_kv_get_int(line, "sim", &dev->sim);
    hupi_kv_get_int(line, "apple", &dev->apple);
    return 0;
}
