#include "hotplug.h"

#include "classifier.h"
#include "log.h"

#include <libudev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

struct usb_udev_monitor {
    struct udev* udev;
    struct udev_monitor* monitor;
    usb_hotplug_cb callback;
    void* user;
    int running;
};

static uint16_t parse_hex(const char* text)
{
    unsigned value = 0;

    if (!text || !text[0]) {
        return 0;
    }
    sscanf(text, "%x", &value);
    return (uint16_t)value;
}

static void copy_str(char* dst, size_t len, const char* src)
{
    if (!dst || len == 0) {
        return;
    }
    snprintf(dst, len, "%s", src ? src : "");
}

static void fill_device(struct udev_device* dev, usb_device_t* info)
{
    const char* syspath = udev_device_get_syspath(dev);
    const char* node = udev_device_get_devnode(dev);
    const char* vendor = udev_device_get_property_value(dev, "ID_VENDOR_ID");
    const char* product = udev_device_get_property_value(dev, "ID_MODEL_ID");
    const char* manufacturer = udev_device_get_property_value(dev, "ID_VENDOR");
    const char* model = udev_device_get_property_value(dev, "ID_MODEL");
    const char* serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
    const char* interfaces = udev_device_get_property_value(dev, "ID_USB_INTERFACES");

    memset(info, 0, sizeof(*info));
    copy_str(info->sys_path, sizeof(info->sys_path), syspath);
    copy_str(info->dev_node, sizeof(info->dev_node), node);
    if (!vendor) {
        vendor = udev_device_get_sysattr_value(dev, "idVendor");
    }
    if (!product) {
        product = udev_device_get_sysattr_value(dev, "idProduct");
    }
    info->vendor_id = parse_hex(vendor);
    info->product_id = parse_hex(product);
    if (!manufacturer) {
        manufacturer = udev_device_get_sysattr_value(dev, "manufacturer");
    }
    if (!model) {
        model = udev_device_get_sysattr_value(dev, "product");
    }
    if (!serial) {
        serial = udev_device_get_sysattr_value(dev, "serial");
    }
    copy_str(info->manufacturer, sizeof(info->manufacturer), manufacturer);
    copy_str(info->product, sizeof(info->product), model);
    copy_str(info->serial, sizeof(info->serial), serial);
    copy_str(info->interfaces, sizeof(info->interfaces), interfaces);
    info->state = USB_STATE_ENUMERATING;
    usb_classify_enrich(info);
    usb_assign_id(info);
}

static int is_usb_device(struct udev_device* dev)
{
    const char* subsystem = udev_device_get_subsystem(dev);
    const char* devtype = udev_device_get_devtype(dev);

    return subsystem && strcmp(subsystem, "usb") == 0 && devtype && strcmp(devtype, "usb_device") == 0;
}

usb_udev_monitor_t* usb_udev_monitor_create(void)
{
    return calloc(1, sizeof(usb_udev_monitor_t));
}

void usb_udev_monitor_destroy(usb_udev_monitor_t* monitor)
{
    if (!monitor) {
        return;
    }
    if (monitor->monitor) {
        udev_monitor_unref(monitor->monitor);
    }
    if (monitor->udev) {
        udev_unref(monitor->udev);
    }
    free(monitor);
}

void usb_udev_monitor_set_callback(usb_udev_monitor_t* monitor, usb_hotplug_cb cb, void* user)
{
    if (!monitor) {
        return;
    }
    monitor->callback = cb;
    monitor->user = user;
}

int usb_udev_monitor_start(usb_udev_monitor_t* monitor)
{
    if (!monitor) {
        return 0;
    }
    if (monitor->running) {
        return 1;
    }
    monitor->udev = udev_new();
    if (!monitor->udev) {
        USB_LOG_ERROR("udev_new failed");
        return 0;
    }
    monitor->monitor = udev_monitor_new_from_netlink(monitor->udev, "udev");
    if (!monitor->monitor) {
        USB_LOG_ERROR("udev_monitor_new_from_netlink failed");
        udev_unref(monitor->udev);
        monitor->udev = NULL;
        return 0;
    }
    udev_monitor_filter_add_match_subsystem_devtype(monitor->monitor, "usb", "usb_device");
    udev_monitor_enable_receiving(monitor->monitor);
    monitor->running = 1;
    USB_LOG_INFO("USB hotplug monitor started");
    return 1;
}

int usb_udev_monitor_poll(usb_udev_monitor_t* monitor, int timeout_ms)
{
    int fd;
    fd_set fds;
    struct timeval tv;
    struct timeval* wait;
    int selected;
    struct udev_device* dev;
    const char* action;
    usb_hotplug_action_t kind;
    usb_device_t info;
    char msg[256];

    if (!monitor || !monitor->monitor || !monitor->callback) {
        return 0;
    }
    fd = udev_monitor_get_fd(monitor->monitor);
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    wait = timeout_ms < 0 ? NULL : &tv;
    selected = select(fd + 1, &fds, NULL, NULL, wait);
    if (selected <= 0 || !FD_ISSET(fd, &fds)) {
        return 0;
    }

    dev = udev_monitor_receive_device(monitor->monitor);
    if (!dev) {
        return 0;
    }
    if (!is_usb_device(dev)) {
        udev_device_unref(dev);
        return 0;
    }
    action = udev_device_get_action(dev);
    kind = USB_HOTPLUG_ADD;
    if (action && strcmp(action, "remove") == 0) {
        kind = USB_HOTPLUG_REMOVE;
    } else if (action && strcmp(action, "change") == 0) {
        kind = USB_HOTPLUG_CHANGE;
    }
    fill_device(dev, &info);
    snprintf(msg, sizeof(msg), "hotplug %s %s type=%s vid=%04x pid=%04x",
             kind == USB_HOTPLUG_REMOVE ? "remove" : (kind == USB_HOTPLUG_CHANGE ? "change" : "add"),
             info.device_id, usb_type_str(info.type), info.vendor_id, info.product_id);
    USB_LOG_INFO(msg);
    monitor->callback(kind, &info, monitor->user);
    udev_device_unref(dev);
    return 1;
}

void usb_udev_monitor_enumerate(usb_udev_monitor_t* monitor)
{
    struct udev_enumerate* enumerate;
    struct udev_list_entry* devices;
    struct udev_list_entry* entry;

    if (!monitor || !monitor->udev || !monitor->callback) {
        return;
    }
    enumerate = udev_enumerate_new(monitor->udev);
    if (!enumerate) {
        return;
    }
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(monitor->udev, path);

        if (!dev) {
            continue;
        }
        if (is_usb_device(dev)) {
            usb_device_t info;
            fill_device(dev, &info);
            monitor->callback(USB_HOTPLUG_ADD, &info, monitor->user);
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
}
