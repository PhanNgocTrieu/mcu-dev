#ifndef CM_USB_MANAGER_H
#define CM_USB_MANAGER_H

#include "adapter.h"
#include "policy.h"
#include "registry.h"

/*
 * Điều phối USB trong connectivity manager.
 *
 * Plug:
 *   udev add -> phân loại -> probe (điện thoại) hoặc mount (ổ đĩa) -> Ready
 *   Android / iPhone: gọi on_projection. Service AA hoặc CarPlay tự StartSession.
 *   Mass storage: mount xong, on_device báo mount_point. UI chỉ hiển thị.
 *
 * Unplug:
 *   dừng phiên nếu đúng thiết bị đó, unmount nếu có, xóa khỏi registry.
 *
 * Callback không được gọi lại usb_manager_*: chúng chạy trong lúc giữ mutex.
 */

typedef struct {
    void (*on_device)(const usb_device_t* device, const char* reason, void* user);
    void (*on_session)(const usb_session_t* session, void* user);
    void (*on_projection)(const char* device_id, const char* mode, void* user);
    void* user;
} usb_manager_listener_t;

typedef struct usb_manager usb_manager_t;

usb_manager_t* usb_manager_create(usb_registry_t* registry, usb_policy_t* policy);
void usb_manager_destroy(usb_manager_t* manager);
void usb_manager_set_adapters(usb_manager_t* manager, usb_adapter_t* android_auto,
                              usb_adapter_t* carplay);
void usb_manager_set_listener(usb_manager_t* manager, const usb_manager_listener_t* listener);

void usb_manager_on_added(usb_manager_t* manager, const usb_device_t* device);
void usb_manager_on_changed(usb_manager_t* manager, const usb_device_t* device);
void usb_manager_on_removed(usb_manager_t* manager, const usb_device_t* device);

int usb_manager_start_session(usb_manager_t* manager, const char* device_id,
                              usb_session_mode_t mode, char* error_out, size_t error_len);
int usb_manager_stop_session(usb_manager_t* manager, const char* device_id,
                             char* error_out, size_t error_len);
int usb_manager_active(const usb_manager_t* manager, usb_session_t* out);

#endif
