#include "classifier.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int contains_ci(const char* hay, const char* needle)
{
    size_t needle_len;
    const char* cursor;

    if (!hay || !needle || !needle[0]) {
        return 0;
    }
    needle_len = strlen(needle);
    for (cursor = hay; *cursor; ++cursor) {
        size_t i = 0;
        while (i < needle_len && cursor[i] &&
               tolower((unsigned char)cursor[i]) == tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) {
            return 1;
        }
    }
    return 0;
}

/* ID_USB_INTERFACES dạng ":030101:080650:". Class là hai ký tự hex sau dấu ':'. */
static int has_class(const char* interfaces, char hi, char lo)
{
    const char* cursor;

    if (!interfaces) {
        return 0;
    }
    for (cursor = interfaces; *cursor; ++cursor) {
        if (*cursor == ':' && isxdigit((unsigned char)cursor[1]) &&
            isxdigit((unsigned char)cursor[2]) &&
            tolower((unsigned char)cursor[1]) == tolower((unsigned char)hi) &&
            tolower((unsigned char)cursor[2]) == tolower((unsigned char)lo)) {
            return 1;
        }
    }
    return 0;
}

static int has_iface_prefix(const char* interfaces, const char* six_hex)
{
    char needle[16];

    if (!interfaces || !six_hex) {
        return 0;
    }
    snprintf(needle, sizeof(needle), ":%s", six_hex);
    return contains_ci(interfaces, needle);
}

static int is_apple(uint16_t vendor_id)
{
    return vendor_id == 0x05ac;
}

static int known_android_vendor(uint16_t vendor_id)
{
    switch (vendor_id) {
    case 0x18d1: /* Google */
    case 0x04e8: /* Samsung */
    case 0x22b8: /* Motorola */
    case 0x0bb4: /* HTC */
    case 0x12d1: /* Huawei */
    case 0x2717: /* Xiaomi */
    case 0x2a47: /* OnePlus / Oppo family thấy trên một số máy */
    case 0x05c6: /* Qualcomm */
        return 1;
    default:
        return 0;
    }
}

static int looks_like_android(const usb_device_t* info)
{
    if (is_apple(info->vendor_id)) {
        return 0;
    }
    if (known_android_vendor(info->vendor_id)) {
        return 1;
    }
    /* MTP/PTP là class 06. ADB là ff4201. AOAP accessory là ffff00. */
    if (has_class(info->interfaces, '0', '6')) {
        return 1;
    }
    if (has_iface_prefix(info->interfaces, "ff4201") ||
        has_iface_prefix(info->interfaces, "ffff00")) {
        return 1;
    }
    if (contains_ci(info->manufacturer, "Google") || contains_ci(info->manufacturer, "Android") ||
        contains_ci(info->product, "Android") || contains_ci(info->product, "MTP") ||
        contains_ci(info->product, "ADB")) {
        return 1;
    }
    return 0;
}

static int looks_like_mass_storage(const usb_device_t* info)
{
    if (has_class(info->interfaces, '0', '8')) {
        return 1;
    }
    if (contains_ci(info->product, "USB Disk") || contains_ci(info->product, "Mass Storage")) {
        return 1;
    }
    return 0;
}

usb_device_type_t usb_classify(const usb_device_t* info)
{
    if (!info) {
        return USB_TYPE_UNKNOWN;
    }
    if (is_apple(info->vendor_id)) {
        return USB_TYPE_IPHONE;
    }
    if (looks_like_android(info)) {
        return USB_TYPE_ANDROID;
    }
    if (looks_like_mass_storage(info)) {
        return USB_TYPE_MASS_STORAGE;
    }
    if (has_class(info->interfaces, '0', '3')) {
        return USB_TYPE_HID;
    }
    return USB_TYPE_UNKNOWN;
}

void usb_classify_enrich(usb_device_t* info)
{
    if (!info) {
        return;
    }
    info->type = usb_classify(info);
}
