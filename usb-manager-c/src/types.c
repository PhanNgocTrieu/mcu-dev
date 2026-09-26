#include "types.h"

#include <string.h>

const char* usb_type_str(usb_device_type_t t) {
  switch (t) {
    case USB_TYPE_ANDROID: return "android";
    case USB_TYPE_IPHONE: return "iphone";
    case USB_TYPE_MASS_STORAGE: return "mass_storage";
    case USB_TYPE_HID: return "hid";
    case USB_TYPE_UNKNOWN: break;
  }
  return "unknown";
}

const char* usb_state_str(usb_device_state_t s) {
  switch (s) {
    case USB_STATE_ENUMERATING: return "enumerating";
    case USB_STATE_CLASSIFIED: return "classified";
    case USB_STATE_PROBING: return "probing";
    case USB_STATE_READY: return "ready";
    case USB_STATE_CONNECTING: return "connecting";
    case USB_STATE_ACTIVE: return "active";
    case USB_STATE_FAILED: return "failed";
    case USB_STATE_DISCONNECTING: return "disconnecting";
    case USB_STATE_IGNORED: return "ignored";
    case USB_STATE_IDLE: break;
  }
  return "idle";
}

const char* usb_mode_str(usb_session_mode_t m) {
  switch (m) {
    case USB_MODE_ANDROID_AUTO: return "android_auto";
    case USB_MODE_CARPLAY: return "carplay";
    case USB_MODE_STORAGE: return "storage";
    case USB_MODE_NONE: break;
  }
  return "none";
}

const char* usb_session_state_str(usb_session_state_t s) {
  switch (s) {
    case USB_SESSION_STARTING: return "starting";
    case USB_SESSION_ACTIVE: return "active";
    case USB_SESSION_STOPPING: return "stopping";
    case USB_SESSION_FAILED: return "failed";
    case USB_SESSION_IDLE: break;
  }
  return "idle";
}

int usb_mode_from_str(const char* s, usb_session_mode_t* out) {
  if (!s || !out) return -1;
  if (strcmp(s, "android_auto") == 0) {
    *out = USB_MODE_ANDROID_AUTO;
    return 0;
  }
  if (strcmp(s, "carplay") == 0) {
    *out = USB_MODE_CARPLAY;
    return 0;
  }
  if (strcmp(s, "storage") == 0) {
    *out = USB_MODE_STORAGE;
    return 0;
  }
  return -1;
}
