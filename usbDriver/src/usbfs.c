#define _GNU_SOURCE
#include "usbdrv.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int usbdrv_usbfs_open(const char *devnode)
{
    int fd;
    if (devnode == NULL || devnode[0] == '\0') {
        return -1;
    }
    fd = open(devnode, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }
    return fd;
}

int usbdrv_usbfs_control(int fd, const usbdrv_control_t *setup, void *data, int timeout_ms)
{
    struct usbdevfs_ctrltransfer ctrl;
    int rc;
    if (fd < 0 || setup == NULL) {
        return -1;
    }
    if (setup->w_length > 0 && data == NULL) {
        return -1;
    }
    memset(&ctrl, 0, sizeof ctrl);
    ctrl.bRequestType = setup->bm_request_type;
    ctrl.bRequest = setup->b_request;
    ctrl.wValue = setup->w_value;
    ctrl.wIndex = setup->w_index;
    ctrl.wLength = setup->w_length;
    ctrl.timeout = timeout_ms > 0 ? (unsigned int)timeout_ms : 1000u;
    ctrl.data = data;
    rc = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    if (rc < 0) {
        return -errno;
    }
    return rc;
}
