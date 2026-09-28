#ifndef CM_USB_REGISTRY_H
#define CM_USB_REGISTRY_H

#include "types.h"

#define USB_MAX_DEVICES 32

/*
 * Danh sách thiết bị đang cắm. Mọi hàm tự khóa.
 * Khóa này không được giữ rồi gọi ngược ra USB manager.
 */
typedef struct usb_registry usb_registry_t;

usb_registry_t* usb_registry_create(void);
void usb_registry_destroy(usb_registry_t* registry);

void usb_registry_upsert(usb_registry_t* registry, const usb_device_t* device);
int usb_registry_remove(usb_registry_t* registry, const char* device_id);
int usb_registry_get(usb_registry_t* registry, const char* device_id, usb_device_t* out);
int usb_registry_list(usb_registry_t* registry, usb_device_t* out, int max);

/*
 * Tìm thiết bị đã biết khi udev gửi remove/change.
 * sys_path là khóa chính vì remove vẫn còn đường dẫn sysfs,
 * kể cả khi serial không có trên event đó.
 */
int usb_registry_lookup(usb_registry_t* registry, const usb_device_t* key, usb_device_t* out);

#endif
