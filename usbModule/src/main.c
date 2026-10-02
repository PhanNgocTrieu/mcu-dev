#include "usbmod.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int want_rescan(const usbdrv_uevent_t *event)
{
    if (strcmp(event->subsystem, "usb") == 0 || strcmp(event->subsystem, "net") == 0 ||
        strcmp(event->subsystem, "block") == 0) {
        return 1;
    }
    return 0;
}

static void on_device(const usbmod_device_t *device, const char *event, void *user)
{
    usbmod_t *mod = user;
    char error[USBMOD_ERR_LEN];
    printf("%s id=%s kind=%s state=%s mode=%s vid=%04x pid=%04x aoa=%d airplay=%d iface=%s block=%s (%s)\n",
           event, device->device_id, usbmod_kind_str(device->kind), usbmod_state_str(device->state),
           usbmod_mode_str(device->mode), device->vendor_id, device->product_id, device->aoa_protocol,
           device->airplay_ready, device->net_iface[0] ? device->net_iface : "-",
           device->block_dev[0] ? device->block_dev : "-", device->reason);
    fflush(stdout);
    if (device->state != USBMOD_STATE_READY) {
        return;
    }
    if (strcmp(event, "added") != 0 && strcmp(event, "updated") != 0) {
        return;
    }
    if (device->kind == USBMOD_KIND_APPLE && device->airplay_ready) {
        usbmod_start(mod, device->device_id, USBMOD_MODE_AIRPLAY, error, sizeof error);
    } else if (device->kind == USBMOD_KIND_ANDROID && device->aoa_protocol >= 1) {
        usbmod_start(mod, device->device_id, USBMOD_MODE_AOA, error, sizeof error);
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr, "usage: %s [--sysfs PATH] [--once] [--no-aoa]\n", argv0);
}

int main(int argc, char **argv)
{
    const char *sysfs = "/sys/bus/usb/devices";
    int once = 0;
    int aoa = 1;
    int i;
    usbmod_t *mod;
    usbmod_listener_t listener;
    int fd;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) {
            once = 1;
        } else if (strcmp(argv[i], "--no-aoa") == 0) {
            aoa = 0;
        } else if (strcmp(argv[i], "--sysfs") == 0 && i + 1 < argc) {
            sysfs = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    mod = usbmod_create();
    if (mod == NULL) {
        fprintf(stderr, "usb-module: out of memory\n");
        return 1;
    }
    usbmod_set_switch_aoa(mod, aoa);
    memset(&listener, 0, sizeof listener);
    listener.on_device = on_device;
    listener.user = mod;
    usbmod_set_listener(mod, &listener);
    if (usbmod_scan(mod, sysfs) != 0) {
        fprintf(stderr, "usb-module: cannot read %s\n", sysfs);
        usbmod_destroy(mod);
        return 1;
    }
    if (once) {
        usbmod_destroy(mod);
        return 0;
    }
    fd = usbdrv_uevent_open();
    if (fd < 0) {
        fprintf(stderr, "usb-module: uevent socket failed (%d)\n", fd);
        usbmod_destroy(mod);
        return 1;
    }
    for (;;) {
        struct pollfd pfd;
        usbdrv_uevent_t event;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, -1) < 0) {
            break;
        }
        if (usbdrv_uevent_recv(fd, &event) != 0) {
            continue;
        }
        if (want_rescan(&event)) {
            usbmod_scan(mod, sysfs);
        }
    }
    close(fd);
    usbmod_destroy(mod);
    return 0;
}
