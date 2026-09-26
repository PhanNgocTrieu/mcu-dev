#ifndef USB_MGR_UDEV_MONITOR_H
#define USB_MGR_UDEV_MONITOR_H

#include "types.h"

typedef enum { USB_HOTPLUG_ADD = 0, USB_HOTPLUG_REMOVE, USB_HOTPLUG_CHANGE } usb_hotplug_action_t;

typedef void (*usb_hotplug_cb)(usb_hotplug_action_t action, const usb_device_t* device, void* user);

typedef struct usb_udev_monitor usb_udev_monitor_t;

usb_udev_monitor_t* usb_udev_monitor_create(void);
void usb_udev_monitor_destroy(usb_udev_monitor_t* mon);
void usb_udev_monitor_set_callback(usb_udev_monitor_t* mon, usb_hotplug_cb cb, void* user);
int usb_udev_monitor_start(usb_udev_monitor_t* mon);
int usb_udev_monitor_poll(usb_udev_monitor_t* mon, int timeout_ms);
void usb_udev_monitor_enumerate(usb_udev_monitor_t* mon);

#endif
