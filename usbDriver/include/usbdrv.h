#ifndef USBDRV_H
#define USBDRV_H

/*
 * USB Driver — biên với stack USB Host của Linux.
 *
 * Module này không thay usbcore hay class driver. Nó đọc những gì kernel đã
 * làm (sysfs, uevent, usbfs) để USB Module phía trên thấy enumeration, driver
 * đã bind, và gửi được control transfer (AOA nằm ở USB Module).
 */

#include <stddef.h>
#include <stdint.h>

#define USBDRV_NAME_LEN 64
#define USBDRV_PATH_LEN 256
#define USBDRV_STR_LEN 128
#define USBDRV_LIST_LEN 192

typedef struct {
    char sys_name[USBDRV_NAME_LEN];
    char sys_path[USBDRV_PATH_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
    char manufacturer[USBDRV_STR_LEN];
    char product[USBDRV_STR_LEN];
    char serial[USBDRV_STR_LEN];
    /* "02/0d/00 ff/42/01" — class/subclass/protocol của từng interface. */
    char interfaces[USBDRV_LIST_LEN];
    char drivers[USBDRV_LIST_LEN];
    int busnum;
    int devnum;
    char devnode[USBDRV_PATH_LEN];
    int class_ncm;
    int class_ecm;
    int class_rndis;
    int class_ipheth;
    int class_storage;
    int class_hid;
    int class_acm;
    int class_adb;
    int class_aoap;
    char net_iface[USBDRV_NAME_LEN];
    char block_dev[USBDRV_NAME_LEN];
} usbdrv_device_t;

typedef struct {
    uint8_t bm_request_type;
    uint8_t b_request;
    uint16_t w_value;
    uint16_t w_index;
    uint16_t w_length;
} usbdrv_control_t;

typedef enum {
    USBDRV_UEVENT_OTHER = 0,
    USBDRV_UEVENT_ADD,
    USBDRV_UEVENT_REMOVE,
    USBDRV_UEVENT_CHANGE
} usbdrv_uevent_action_t;

typedef struct {
    usbdrv_uevent_action_t action;
    char subsystem[USBDRV_NAME_LEN];
    char devpath[USBDRV_PATH_LEN];
    char product[USBDRV_STR_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
    char devname[USBDRV_PATH_LEN];
} usbdrv_uevent_t;

/* Điền out[0..capacity). *count là số bản ghi đã ghi. 0 nếu đọc được thư mục. */
int usbdrv_enum_root(const char *sysfs_devices, usbdrv_device_t *out, size_t capacity,
                     size_t *count);

/* text có thể chứa NUL giữa các biến uevent. len là số byte hợp lệ. */
int usbdrv_uevent_parse(const char *text, size_t len, usbdrv_uevent_t *out);
const char *usbdrv_action_str(usbdrv_uevent_action_t action);

int usbdrv_uevent_open(void);
int usbdrv_uevent_recv(int fd, usbdrv_uevent_t *out);

int usbdrv_usbfs_open(const char *devnode);
int usbdrv_usbfs_control(int fd, const usbdrv_control_t *setup, void *data, int timeout_ms);

#endif
