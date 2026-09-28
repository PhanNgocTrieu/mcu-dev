#include "types.h"

#include <stdio.h>
#include <string.h>

const char* usb_type_str(usb_device_type_t type)
{
    switch (type) {
    case USB_TYPE_ANDROID:
        return "android";
    case USB_TYPE_IPHONE:
        return "iphone";
    case USB_TYPE_MASS_STORAGE:
        return "mass_storage";
    case USB_TYPE_HID:
        return "hid";
    case USB_TYPE_UNKNOWN:
        break;
    }
    return "unknown";
}

const char* usb_state_str(usb_device_state_t state)
{
    switch (state) {
    case USB_STATE_ENUMERATING:
        return "enumerating";
    case USB_STATE_CLASSIFIED:
        return "classified";
    case USB_STATE_PROBING:
        return "probing";
    case USB_STATE_READY:
        return "ready";
    case USB_STATE_CONNECTING:
        return "connecting";
    case USB_STATE_ACTIVE:
        return "active";
    case USB_STATE_FAILED:
        return "failed";
    case USB_STATE_DISCONNECTING:
        return "disconnecting";
    case USB_STATE_IGNORED:
        return "ignored";
    case USB_STATE_IDLE:
        break;
    }
    return "idle";
}

const char* usb_mode_str(usb_session_mode_t mode)
{
    switch (mode) {
    case USB_MODE_ANDROID_AUTO:
        return "android_auto";
    case USB_MODE_CARPLAY:
        return "carplay";
    case USB_MODE_STORAGE:
        return "storage";
    case USB_MODE_NONE:
        break;
    }
    return "none";
}

const char* usb_session_state_str(usb_session_state_t state)
{
    switch (state) {
    case USB_SESSION_STARTING:
        return "starting";
    case USB_SESSION_ACTIVE:
        return "active";
    case USB_SESSION_STOPPING:
        return "stopping";
    case USB_SESSION_FAILED:
        return "failed";
    case USB_SESSION_IDLE:
        break;
    }
    return "idle";
}

int usb_mode_from_str(const char* text, usb_session_mode_t* out)
{
    if (!text || !out) {
        return -1;
    }
    if (strcmp(text, "android_auto") == 0) {
        *out = USB_MODE_ANDROID_AUTO;
        return 0;
    }
    if (strcmp(text, "carplay") == 0) {
        *out = USB_MODE_CARPLAY;
        return 0;
    }
    if (strcmp(text, "storage") == 0) {
        *out = USB_MODE_STORAGE;
        return 0;
    }
    return -1;
}

void usb_assign_id(usb_device_t* device)
{
    const char* slash;
    const char* port;

    if (!device) {
        return;
    }
    if (device->serial[0]) {
        snprintf(device->device_id, sizeof(device->device_id), "usb-%04x-%04x-%s",
                 device->vendor_id, device->product_id, device->serial);
        return;
    }
    slash = device->sys_path[0] ? strrchr(device->sys_path, '/') : NULL;
    port = slash ? slash + 1 : (device->sys_path[0] ? device->sys_path : "unknown");
    /* Cổng USB dạng "1-1.2" rất ngắn. Cắt để device_id luôn vừa USB_ID_LEN. */
    snprintf(device->device_id, sizeof(device->device_id), "port-%.120s-%04x-%04x", port,
             device->vendor_id, device->product_id);
}
