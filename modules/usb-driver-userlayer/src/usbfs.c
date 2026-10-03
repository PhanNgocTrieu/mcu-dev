/**
 * @file usbfs.c
 * @brief Wrapper mở /dev/bus/usb/... và gửi USB control transfer (USBDEVFS_CONTROL).
 *
 * Dùng cho AOA / vendor request từ usb-driverd thay vì để nhiều process cùng claim.
 */
#include "usbdrv.h"

#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int usbdrv_usbfs_open(const char *devnode)
{
    if (!devnode || !devnode[0]) {
        return -1;
    }
    return open(devnode, O_RDWR | O_CLOEXEC);
}

int usbdrv_usbfs_control(int fd, const usbdrv_control_t *setup, void *data, int timeout_ms)
{
    struct usbdevfs_ctrltransfer ctrl;

    if (fd < 0 || !setup) {
        return -1;
    }
    memset(&ctrl, 0, sizeof ctrl);
    ctrl.bRequestType = setup->bm_request_type;
    ctrl.bRequest = setup->b_request;
    ctrl.wValue = setup->w_value;
    ctrl.wIndex = setup->w_index;
    ctrl.wLength = setup->w_length;
    ctrl.timeout = timeout_ms > 0 ? (unsigned)timeout_ms : 1000;
    ctrl.data = data;
    return ioctl(fd, USBDEVFS_CONTROL, &ctrl);
}
