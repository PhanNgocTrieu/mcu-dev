#define _GNU_SOURCE
#include "usbdrv.h"

#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void copy_token(char *dst, size_t cap, const char *src, size_t n)
{
    size_t i = 0;
    if (cap == 0) {
        return;
    }
    while (i + 1 < cap && i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static usbdrv_uevent_action_t action_from(const char *text)
{
    if (strcmp(text, "add") == 0) {
        return USBDRV_UEVENT_ADD;
    }
    if (strcmp(text, "remove") == 0) {
        return USBDRV_UEVENT_REMOVE;
    }
    if (strcmp(text, "change") == 0) {
        return USBDRV_UEVENT_CHANGE;
    }
    return USBDRV_UEVENT_OTHER;
}

static void parse_product(usbdrv_uevent_t *out)
{
    unsigned vid = 0;
    unsigned pid = 0;
    if (sscanf(out->product, "%x/%x", &vid, &pid) == 2) {
        out->vendor_id = (uint16_t)vid;
        out->product_id = (uint16_t)pid;
    }
}

const char *usbdrv_action_str(usbdrv_uevent_action_t action)
{
    switch (action) {
    case USBDRV_UEVENT_ADD:
        return "add";
    case USBDRV_UEVENT_REMOVE:
        return "remove";
    case USBDRV_UEVENT_CHANGE:
        return "change";
    default:
        return "other";
    }
}

int usbdrv_uevent_parse(const char *text, size_t len, usbdrv_uevent_t *out)
{
    size_t i = 0;
    int first = 1;
    if (text == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->action = USBDRV_UEVENT_OTHER;
    while (i < len) {
        size_t start;
        size_t n;
        char token[USBDRV_PATH_LEN];
        char *eq;
        while (i < len && (text[i] == '\0' || text[i] == '\n')) {
            i++;
        }
        if (i >= len) {
            break;
        }
        start = i;
        while (i < len && text[i] != '\0' && text[i] != '\n') {
            i++;
        }
        n = i - start;
        copy_token(token, sizeof token, text + start, n);
        if (first && strchr(token, '=') == NULL) {
            char *at = strchr(token, '@');
            first = 0;
            if (at != NULL) {
                *at = '\0';
                out->action = action_from(token);
                copy_token(out->devpath, sizeof out->devpath, at + 1, strlen(at + 1));
            }
            continue;
        }
        first = 0;
        eq = strchr(token, '=');
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        if (strcmp(token, "ACTION") == 0) {
            out->action = action_from(eq + 1);
        } else if (strcmp(token, "SUBSYSTEM") == 0) {
            copy_token(out->subsystem, sizeof out->subsystem, eq + 1, strlen(eq + 1));
        } else if (strcmp(token, "DEVPATH") == 0) {
            copy_token(out->devpath, sizeof out->devpath, eq + 1, strlen(eq + 1));
        } else if (strcmp(token, "PRODUCT") == 0) {
            copy_token(out->product, sizeof out->product, eq + 1, strlen(eq + 1));
            parse_product(out);
        } else if (strcmp(token, "DEVNAME") == 0) {
            copy_token(out->devname, sizeof out->devname, eq + 1, strlen(eq + 1));
        }
    }
    return 0;
}

int usbdrv_uevent_open(void)
{
    int fd;
    struct sockaddr_nl addr;
    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (fd < 0) {
        return -errno;
    }
    memset(&addr, 0, sizeof addr);
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = 1;
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        int err = errno;
        close(fd);
        return -err;
    }
    return fd;
}

int usbdrv_uevent_recv(int fd, usbdrv_uevent_t *out)
{
    char buf[4096];
    ssize_t n;
    if (fd < 0 || out == NULL) {
        return -1;
    }
    n = recv(fd, buf, sizeof buf, 0);
    if (n < 0) {
        return -errno;
    }
    return usbdrv_uevent_parse(buf, (size_t)n, out);
}
