#include "usbmod.h"

#include <string.h>

static int phone_vendor(uint16_t vid)
{
    switch (vid) {
    case 0x18d1: /* Google */
    case 0x04e8: /* Samsung */
    case 0x22b8: /* Motorola */
    case 0x2717: /* Xiaomi */
    case 0x0fce: /* Sony */
    case 0x12d1: /* Huawei */
    case 0x1004: /* LG */
    case 0x0bb4: /* HTC */
    case 0x22d9: /* OPPO */
    case 0x2d95: /* vivo */
        return 1;
    default:
        return 0;
    }
}

static int has_network_class(const usbdrv_device_t *device)
{
    return device->class_ncm || device->class_ecm || device->class_rndis || device->class_ipheth ||
           device->net_iface[0] != '\0';
}

usbmod_kind_t usbmod_classify(const usbdrv_device_t *device)
{
    if (device == NULL) {
        return USBMOD_KIND_UNKNOWN;
    }
    if (device->vendor_id == 0x05ac) {
        return USBMOD_KIND_APPLE;
    }
    if (device->class_aoap || device->class_adb || phone_vendor(device->vendor_id)) {
        return USBMOD_KIND_ANDROID;
    }
    if (has_network_class(device)) {
        return USBMOD_KIND_NETWORK;
    }
    if (device->class_storage) {
        return USBMOD_KIND_STORAGE;
    }
    if (device->class_hid) {
        return USBMOD_KIND_HID;
    }
    return USBMOD_KIND_UNKNOWN;
}

int usbmod_airplay_ready(const usbdrv_device_t *device)
{
    if (device == NULL || device->vendor_id != 0x05ac) {
        return 0;
    }
    return has_network_class(device);
}

const char *usbmod_kind_str(usbmod_kind_t kind)
{
    switch (kind) {
    case USBMOD_KIND_ANDROID:
        return "android";
    case USBMOD_KIND_APPLE:
        return "apple";
    case USBMOD_KIND_STORAGE:
        return "storage";
    case USBMOD_KIND_HID:
        return "hid";
    case USBMOD_KIND_NETWORK:
        return "network";
    default:
        return "unknown";
    }
}

const char *usbmod_state_str(usbmod_state_t state)
{
    switch (state) {
    case USBMOD_STATE_ENUMERATING:
        return "enumerating";
    case USBMOD_STATE_CLASSIFIED:
        return "classified";
    case USBMOD_STATE_PROBING:
        return "probing";
    case USBMOD_STATE_READY:
        return "ready";
    case USBMOD_STATE_ACTIVE:
        return "active";
    case USBMOD_STATE_FAILED:
        return "failed";
    case USBMOD_STATE_IGNORED:
        return "ignored";
    default:
        return "unknown";
    }
}

const char *usbmod_mode_str(usbmod_mode_t mode)
{
    switch (mode) {
    case USBMOD_MODE_AOA:
        return "aoa";
    case USBMOD_MODE_AIRPLAY:
        return "airplay";
    case USBMOD_MODE_STORAGE:
        return "storage";
    default:
        return "none";
    }
}

int usbmod_mode_from_str(const char *text, usbmod_mode_t *out)
{
    if (text == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(text, "aoa") == 0) {
        *out = USBMOD_MODE_AOA;
        return 0;
    }
    if (strcmp(text, "airplay") == 0) {
        *out = USBMOD_MODE_AIRPLAY;
        return 0;
    }
    if (strcmp(text, "storage") == 0) {
        *out = USBMOD_MODE_STORAGE;
        return 0;
    }
    return -1;
}
