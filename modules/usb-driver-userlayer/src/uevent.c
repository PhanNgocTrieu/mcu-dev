/**
 * @file uevent.c
 * @brief Lắng nghe netlink kobject uevent (subsystem usb) để biết cắm/rút.
 *
 * Không thay thế usbcore — chỉ nhận tín hiệu rồi để driverd quét lại sysfs.
 * Payload là các field null-separated: ACTION= SUBSYSTEM= DEVPATH= PRODUCT=...
 */
#include "usbdrv.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>

static void set_field(usbdrv_uevent_t *out, const char *key, const char *val)
{
    if (strcmp(key, "ACTION") == 0) {
        if (strcmp(val, "add") == 0) {
            out->action = USBDRV_UEVENT_ADD;
        } else if (strcmp(val, "remove") == 0) {
            out->action = USBDRV_UEVENT_REMOVE;
        } else if (strcmp(val, "change") == 0) {
            out->action = USBDRV_UEVENT_CHANGE;
        } else {
            out->action = USBDRV_UEVENT_OTHER;
        }
    } else if (strcmp(key, "SUBSYSTEM") == 0) {
        snprintf(out->subsystem, sizeof out->subsystem, "%s", val);
    } else if (strcmp(key, "DEVTYPE") == 0) {
        snprintf(out->devtype, sizeof out->devtype, "%s", val);
    } else if (strcmp(key, "DEVPATH") == 0) {
        snprintf(out->devpath, sizeof out->devpath, "%s", val);
    } else if (strcmp(key, "PRODUCT") == 0) {
        unsigned vid = 0, pid = 0;
        if (sscanf(val, "%x/%x", &vid, &pid) == 2) {
            out->vendor_id = (uint16_t)vid;
            out->product_id = (uint16_t)pid;
        }
    }
}

int usbdrv_uevent_parse(const char *text, size_t len, usbdrv_uevent_t *out)
{
    size_t i = 0;

    if (!text || !out) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    while (i < len) {
        const char *field = text + i;
        size_t flen = strnlen(field, len - i);
        const char *eq;

        if (flen == 0) {
            i++;
            continue;
        }
        eq = memchr(field, '=', flen);
        if (eq) {
            char key[64];
            char val[USBDRV_PATH_LEN];
            size_t klen = (size_t)(eq - field);
            size_t vlen = flen - klen - 1;
            if (klen >= sizeof key) {
                klen = sizeof key - 1;
            }
            if (vlen >= sizeof val) {
                vlen = sizeof val - 1;
            }
            memcpy(key, field, klen);
            key[klen] = '\0';
            memcpy(val, eq + 1, vlen);
            val[vlen] = '\0';
            set_field(out, key, val);
        } else if (strchr(field, '@')) {
            /* Kernel prefix: "add@/devices/...". ACTION may follow. */
            const char *at = strchr(field, '@');
            char action[32];
            size_t alen = (size_t)(at - field);
            if (alen >= sizeof action) {
                alen = sizeof action - 1;
            }
            memcpy(action, field, alen);
            action[alen] = '\0';
            set_field(out, "ACTION", action);
            if (out->devpath[0] == '\0') {
                snprintf(out->devpath, sizeof out->devpath, "%s", at + 1);
            }
        }
        i += flen + 1;
    }
    return 0;
}

/* Bind netlink group 1 = kernel uevent multicast. */
int usbdrv_uevent_open(void)
{
    int fd;
    struct sockaddr_nl addr;

    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = 1;
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* 1 = parse được một event, 0 = EAGAIN/rỗng, -1 = lỗi. */
int usbdrv_uevent_recv(int fd, usbdrv_uevent_t *out)
{
    char buf[2048];
    ssize_t n;

    n = recv(fd, buf, sizeof buf, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    return usbdrv_uevent_parse(buf, (size_t)n, out) == 0 ? 1 : -1;
}
