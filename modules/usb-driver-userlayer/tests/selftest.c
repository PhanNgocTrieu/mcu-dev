#include "hupi_wire.h"
#include "usbdrv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        exit(1);
    }
    fputs(text, fp);
    fclose(fp);
}

static void test_enum(void)
{
    char root[] = "/tmp/usbdrv-sysfs-XXXXXX";
    char path[256];
    usbdrv_device_t devs[8];
    size_t count = 0;

    if (!mkdtemp(root)) {
        perror("mkdtemp");
        exit(1);
    }
    snprintf(path, sizeof path, "%s/usb1", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/usb1/idVendor", root);
    write_file(path, "1d6b\n");
    snprintf(path, sizeof path, "%s/usb1/idProduct", root);
    write_file(path, "0002\n");

    snprintf(path, sizeof path, "%s/1-1", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/1-1/idVendor", root);
    write_file(path, "05ac\n");
    snprintf(path, sizeof path, "%s/1-1/idProduct", root);
    write_file(path, "12a8\n");
    snprintf(path, sizeof path, "%s/1-1/manufacturer", root);
    write_file(path, "Apple\n");
    snprintf(path, sizeof path, "%s/1-1/product", root);
    write_file(path, "iPhone\n");
    snprintf(path, sizeof path, "%s/1-1/serial", root);
    write_file(path, "SIM\n");
    snprintf(path, sizeof path, "%s/1-1/busnum", root);
    write_file(path, "1\n");
    snprintf(path, sizeof path, "%s/1-1/devnum", root);
    write_file(path, "4\n");
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0/bInterfaceClass", root);
    write_file(path, "02\n");
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0/bInterfaceSubClass", root);
    write_file(path, "0d\n");
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0/bInterfaceProtocol", root);
    write_file(path, "00\n");
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0/net", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/1-1/1-1:1.0/net/usb0", root);
    mkdir(path, 0755);

    expect(usbdrv_enum_root(root, devs, 8, &count) == 0, "enum opens");
    expect(count == 1, "hub is skipped");
    expect(devs[0].vendor_id == 0x05ac, "vid");
    expect(devs[0].product_id == 0x12a8, "pid");
    expect(devs[0].class_ncm == 1, "ncm");
    expect(strcmp(devs[0].net_iface, "usb0") == 0, "net");
    expect(devs[0].busnum == 1 && devs[0].devnum == 4, "bus/dev");
    expect(strcmp(devs[0].devnode, "/dev/bus/usb/001/004") == 0, "node");
}

static void test_uevent(void)
{
    const char raw[] = "add@/devices/usb/1-1\0ACTION=add\0SUBSYSTEM=usb\0DEVTYPE=usb_device\0PRODUCT=18d1/4ee2/410\0";
    usbdrv_uevent_t ev;
    size_t len = sizeof raw - 1;
    expect(usbdrv_uevent_parse(raw, len, &ev) == 0, "parse");
    expect(ev.action == USBDRV_UEVENT_ADD, "add");
    expect(strcmp(ev.subsystem, "usb") == 0, "subsystem");
    expect(ev.vendor_id == 0x18d1 && ev.product_id == 0x4ee2, "product");
}

static void test_escape(void)
{
    char enc[64];
    char dec[64];
    hupi_escape("Raspberry Pi 4", enc, sizeof enc);
    hupi_unescape(enc, dec, sizeof dec);
    expect(strcmp(dec, "Raspberry Pi 4") == 0, "roundtrip");
    hupi_escape("", enc, sizeof enc);
    expect(strcmp(enc, "-") == 0, "empty token");
}

int main(void)
{
    test_enum();
    test_uevent();
    test_escape();
    if (g_fail) {
        fprintf(stderr, "usb-driver selftest failed\n");
        return 1;
    }
    printf("usb-driver selftest ok\n");
    return 0;
}
