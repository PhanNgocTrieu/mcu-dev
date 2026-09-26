#include "classifier.h"

#include <ctype.h>
#include <string.h>

static int contains_ci(const char* hay, const char* needle) {
  if (!hay || !needle || !*needle) return 0;
  size_t nlen = strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    while (i < nlen && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      ++i;
    }
    if (i == nlen) return 1;
  }
  return 0;
}

static int contains_substr(const char* hay, const char* needle) {
  if (!hay || !needle) return 0;
  return strstr(hay, needle) != NULL;
}

static int is_apple(uint16_t vid) { return vid == 0x05ac; }

static int looks_like_mass_storage(const usb_device_t* info) {
  if (contains_substr(info->interfaces, ":08")) return 1;
  if (contains_ci(info->product, "USB Disk") || contains_ci(info->product, "Mass Storage")) return 1;
  return 0;
}

static int looks_like_hid(const usb_device_t* info) {
  return contains_substr(info->interfaces, ":03");
}

static int looks_like_android(const usb_device_t* info) {
  if (is_apple(info->vendor_id)) return 0;
  if (contains_ci(info->manufacturer, "Google") || contains_ci(info->manufacturer, "Android") ||
      contains_ci(info->product, "Android") || contains_ci(info->product, "MTP") ||
      contains_ci(info->product, "ADB")) {
    return 1;
  }
  if (contains_substr(info->interfaces, ":ff") || contains_substr(info->interfaces, ":FF")) {
    return 1;
  }
  switch (info->vendor_id) {
    case 0x18d1:
    case 0x04e8:
    case 0x22b8:
    case 0x0bb4:
    case 0x12d1:
    case 0x2717:
    case 0x2a47:
    case 0x05c6:
      return 1;
    default:
      return 0;
  }
}

usb_device_type_t usb_classify(const usb_device_t* info) {
  if (!info) return USB_TYPE_UNKNOWN;
  if (is_apple(info->vendor_id)) return USB_TYPE_IPHONE;
  if (looks_like_android(info)) return USB_TYPE_ANDROID;
  if (looks_like_mass_storage(info)) return USB_TYPE_MASS_STORAGE;
  if (looks_like_hid(info)) return USB_TYPE_HID;
  return USB_TYPE_UNKNOWN;
}

void usb_classify_enrich(usb_device_t* info) {
  if (!info) return;
  info->type = usb_classify(info);
}
