#ifndef CM_USB_STORAGE_H
#define CM_USB_STORAGE_H

#include "types.h"

/*
 * Mount / unmount mass storage ngay trong USB manager.
 * Chưa có service storage riêng.
 *
 * Ổ được mount tại <root>/<device_id>. Root mặc định là /run/connectivity/usb
 * (systemd RuntimeDirectory tạo sẵn trên board).
 *
 * Selftest thay find/mount/unmount bằng ops giả để không đụng ổ đĩa thật.
 */

typedef struct {
    int (*find_block)(const char* sys_path, char* block_out, size_t block_len);
    int (*mount_fs)(const char* block, const char* target, char* err, size_t err_len);
    int (*unmount_fs)(const char* target, char* err, size_t err_len);
} usb_storage_ops_t;

void usb_storage_set_root(const char* root);
void usb_storage_set_ops(const usb_storage_ops_t* ops);

/* Điền block_dev và mount_point. Trả 1 khi mount xong. */
int usb_storage_attach(usb_device_t* device, char* err, size_t err_len);
int usb_storage_detach(const char* mount_point, char* err, size_t err_len);

#endif
