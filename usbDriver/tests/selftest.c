#include "usbdrv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_failed;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_failed = 1;
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        exit(1);
    }
    fputs(text, file);
    fclose(file);
}

static void make_dir(const char *path)
{
    if (mkdir(path, 0755) != 0) {
        perror(path);
        exit(1);
    }
}

static void build_fixture(char *root, size_t cap)
{
    char path[512];
    char base[] = "/tmp/usbdrv-XXXXXX";
    char *dir = mkdtemp(base);
    if (dir == NULL) {
        perror("mkdtemp");
        exit(1);
    }
    snprintf(root, cap, "%s", dir);

    snprintf(path, sizeof path, "%s/usb2", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-1:1.0", root);
    make_dir(path);

    snprintf(path, sizeof path, "%s/2-1", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-1/idVendor", root);
    write_file(path, "05ac\n");
    snprintf(path, sizeof path, "%s/2-1/idProduct", root);
    write_file(path, "12a8\n");
    snprintf(path, sizeof path, "%s/2-1/manufacturer", root);
    write_file(path, "Apple\n");
    snprintf(path, sizeof path, "%s/2-1/product", root);
    write_file(path, "iPhone\n");
    snprintf(path, sizeof path, "%s/2-1/serial", root);
    write_file(path, "SNAPPLE\n");
    snprintf(path, sizeof path, "%s/2-1/busnum", root);
    write_file(path, "2\n");
    snprintf(path, sizeof path, "%s/2-1/devnum", root);
    write_file(path, "4\n");
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/bInterfaceClass", root);
    write_file(path, "02\n");
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/bInterfaceSubClass", root);
    write_file(path, "0d\n");
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/bInterfaceProtocol", root);
    write_file(path, "00\n");
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/net", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/net/usb0", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-1/2-1:1.0/driver", root);
    if (symlink("cdc_ncm", path) != 0) {
        perror("symlink");
        exit(1);
    }

    snprintf(path, sizeof path, "%s/2-2", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-2/idVendor", root);
    write_file(path, "0781\n");
    snprintf(path, sizeof path, "%s/2-2/idProduct", root);
    write_file(path, "5581\n");
    snprintf(path, sizeof path, "%s/2-2/busnum", root);
    write_file(path, "2\n");
    snprintf(path, sizeof path, "%s/2-2/devnum", root);
    write_file(path, "5\n");
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/bInterfaceClass", root);
    write_file(path, "08\n");
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/bInterfaceSubClass", root);
    write_file(path, "06\n");
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/bInterfaceProtocol", root);
    write_file(path, "50\n");
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/driver", root);
    if (symlink("../../bus/usb/drivers/usb-storage", path) != 0) {
        perror("symlink");
        exit(1);
    }
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/host0", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/host0/block", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-2/2-2:1.0/host0/block/sda", root);
    make_dir(path);

    snprintf(path, sizeof path, "%s/2-3", root);
    make_dir(path);
    snprintf(path, sizeof path, "%s/2-3/idVendor", root);
    write_file(path, "18d1\n");
    snprintf(path, sizeof path, "%s/2-3/idProduct", root);
    write_file(path, "2d00\n");
    snprintf(path, sizeof path, "%s/2-3/busnum", root);
    write_file(path, "2\n");
    snprintf(path, sizeof path, "%s/2-3/devnum", root);
    write_file(path, "6\n");
}

static const usbdrv_device_t *find_name(const usbdrv_device_t *list, size_t count, const char *name)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(list[i].sys_name, name) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static void test_enum(void)
{
    char root[256];
    usbdrv_device_t list[8];
    size_t count = 0;
    const usbdrv_device_t *phone;
    const usbdrv_device_t *stick;
    const usbdrv_device_t *aoap;
    build_fixture(root, sizeof root);
    expect(usbdrv_enum_root(root, list, 8, &count) == 0, "enum returns 0");
    expect(count == 3, "three devices, hubs and bare interfaces skipped");
    phone = find_name(list, count, "2-1");
    stick = find_name(list, count, "2-2");
    aoap = find_name(list, count, "2-3");
    expect(phone != NULL, "iphone present");
    if (phone != NULL) {
        expect(phone->vendor_id == 0x05ac, "apple vid");
        expect(phone->product_id == 0x12a8, "apple pid");
        expect(strcmp(phone->serial, "SNAPPLE") == 0, "serial");
        expect(phone->class_ncm == 1, "ncm class");
        expect(strcmp(phone->net_iface, "usb0") == 0, "net iface");
        expect(strcmp(phone->drivers, "cdc_ncm") == 0, "cdc_ncm driver");
        expect(strcmp(phone->devnode, "/dev/bus/usb/002/004") == 0, "usbfs node");
    }
    expect(stick != NULL, "stick present");
    if (stick != NULL) {
        expect(stick->class_storage == 1, "storage class");
        expect(strcmp(stick->block_dev, "sda") == 0, "block dev");
        expect(strcmp(stick->drivers, "usb-storage") == 0, "usb-storage driver");
    }
    expect(aoap != NULL && aoap->class_aoap == 1, "aoap pid flagged");
}

static size_t push_field(char *buf, size_t used, size_t cap, const char *field)
{
    size_t n = strlen(field);
    if (used + n + 1 > cap) {
        return used;
    }
    memcpy(buf + used, field, n);
    buf[used + n] = '\0';
    return used + n + 1;
}

static void test_uevent(void)
{
    usbdrv_uevent_t ev;
    char buf[256];
    size_t n = 0;
    const char *text = "remove@/devices/usb2/2-1\nSUBSYSTEM=usb\n";
    n = push_field(buf, n, sizeof buf, "add@/devices/pci/usb2/2-1");
    n = push_field(buf, n, sizeof buf, "ACTION=add");
    n = push_field(buf, n, sizeof buf, "SUBSYSTEM=usb");
    n = push_field(buf, n, sizeof buf, "PRODUCT=18d1/2d00/100");
    n = push_field(buf, n, sizeof buf, "DEVNAME=bus/usb/002/006");
    expect(usbdrv_uevent_parse(buf, n, &ev) == 0, "parse uevent");
    expect(ev.action == USBDRV_UEVENT_ADD, "action add");
    expect(strcmp(ev.subsystem, "usb") == 0, "subsystem");
    expect(ev.vendor_id == 0x18d1 && ev.product_id == 0x2d00, "product vid/pid");
    expect(strcmp(ev.devname, "bus/usb/002/006") == 0, "devname");
    expect(strcmp(usbdrv_action_str(ev.action), "add") == 0, "action string");

    expect(usbdrv_uevent_parse(text, strlen(text), &ev) == 0, "text uevent");
    expect(ev.action == USBDRV_UEVENT_REMOVE, "remove action");
    expect(strcmp(ev.subsystem, "usb") == 0, "text subsystem");
}

int main(void)
{
    test_enum();
    test_uevent();
    if (g_failed) {
        fprintf(stderr, "usbdrv-selftest failed\n");
        return 1;
    }
    printf("usbdrv-selftest ok\n");
    return 0;
}
