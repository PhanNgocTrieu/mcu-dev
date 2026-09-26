#ifndef USB_MGR_REGISTRY_H
#define USB_MGR_REGISTRY_H

#include "types.h"

#define USB_MAX_DEVICES 32

typedef void (*usb_device_change_cb)(const usb_device_t* device, const char* reason, void* user);

typedef struct usb_registry usb_registry_t;

usb_registry_t* usb_registry_create(void);
void usb_registry_destroy(usb_registry_t* reg);
void usb_registry_set_callback(usb_registry_t* reg, usb_device_change_cb cb, void* user);
void usb_registry_upsert(usb_registry_t* reg, const usb_device_t* device, const char* reason);
int usb_registry_remove(usb_registry_t* reg, const char* device_id, const char* reason);
int usb_registry_get(usb_registry_t* reg, const char* device_id, usb_device_t* out);
int usb_registry_list(usb_registry_t* reg, usb_device_t* out, int max);
int usb_registry_update(usb_registry_t* reg, const char* device_id,
                        void (*mutator)(usb_device_t* device, void* ctx), void* ctx,
                        const char* reason);

#endif
