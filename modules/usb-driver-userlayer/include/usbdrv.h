/**
 * @file usbdrv.h
 * @brief USB driver userlayer — boundary against the Linux USB host stack.
 *
 * This module does not replace usbcore. It reads what the kernel already
 * enumerated (sysfs, uevent) and performs usbfs control transfers so that
 * @c usb-man can run AOA / claim a device without opening /dev/bus/usb itself
 * from multiple processes.
 */
#ifndef USBDRV_H
#define USBDRV_H

#include <stddef.h>
#include <stdint.h>

#define USBDRV_NAME_LEN 64
#define USBDRV_PATH_LEN 256
#define USBDRV_STR_LEN 128
#define USBDRV_LIST_LEN 192

/** @brief Snapshot of one USB device node under /sys/bus/usb/devices. */
typedef struct {
    char sys_name[USBDRV_NAME_LEN];
    char sys_path[USBDRV_PATH_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
    char manufacturer[USBDRV_STR_LEN];
    char product[USBDRV_STR_LEN];
    char serial[USBDRV_STR_LEN];
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
    int class_adb;
    int class_accessory;
    char net_iface[USBDRV_NAME_LEN];
} usbdrv_device_t;

/** @brief USB setup packet for a control transfer. */
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

/** @brief Parsed netlink kobject uevent (USB subsystem). */
typedef struct {
    usbdrv_uevent_action_t action;
    char subsystem[USBDRV_NAME_LEN];
    char devtype[USBDRV_NAME_LEN];
    char devpath[USBDRV_PATH_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
} usbdrv_uevent_t;

/**
 * @brief Enumerate USB devices under @p sysfs_devices.
 * @return 0 on success (directory readable), -1 on error.
 */
int usbdrv_enum_root(const char *sysfs_devices, usbdrv_device_t *out, size_t capacity,
                     size_t *count);

int usbdrv_uevent_parse(const char *text, size_t len, usbdrv_uevent_t *out);
int usbdrv_uevent_open(void);
/** @return 1 if an event was parsed, 0 if EAGAIN, -1 on error. */
int usbdrv_uevent_recv(int fd, usbdrv_uevent_t *out);

int usbdrv_usbfs_open(const char *devnode);
/** @return transferred byte count, or -1. */
int usbdrv_usbfs_control(int fd, const usbdrv_control_t *setup, void *data, int timeout_ms);

#endif
