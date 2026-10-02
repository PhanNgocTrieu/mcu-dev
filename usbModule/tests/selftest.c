#include "usbmod.h"

#include <stdio.h>
#include <string.h>

static int g_failed;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_failed = 1;
    }
}

static usbdrv_device_t device_of(const char *name, uint16_t vid, uint16_t pid, const char *serial)
{
    usbdrv_device_t device;
    memset(&device, 0, sizeof device);
    snprintf(device.sys_name, sizeof device.sys_name, "%s", name);
    device.vendor_id = vid;
    device.product_id = pid;
    if (serial != NULL) {
        snprintf(device.serial, sizeof device.serial, "%s", serial);
    }
    snprintf(device.devnode, sizeof device.devnode, "/dev/bus/usb/002/010");
    return device;
}

static void test_aoa_plan(void)
{
    usbmod_aoa_strings_t strings = {"Lab", "Meter", "desc", "1.0", "uri", "SER"};
    usbmod_aoa_plan_t plan;
    expect(usbmod_aoa_build(&strings, &plan) == 0, "aoa plan");
    expect(plan.count == 8, "8 aoa steps");
    expect(plan.steps[0].setup.bm_request_type == 0xc0, "get protocol in");
    expect(plan.steps[0].setup.b_request == 51, "request 51");
    expect(plan.steps[0].setup.w_length == 2, "protocol length");
    expect(plan.steps[1].setup.b_request == 52 && plan.steps[1].setup.w_index == 0, "string 0");
    expect(strcmp(plan.steps[1].payload, "Lab") == 0, "manufacturer");
    expect(plan.steps[6].setup.w_index == 5 && strcmp(plan.steps[6].payload, "SER") == 0, "serial string");
    expect(plan.steps[7].setup.b_request == 53 && plan.steps[7].setup.w_length == 0, "start accessory");
}

static void test_sessions(void)
{
    usbmod_t *mod = usbmod_create();
    usbdrv_device_t apple;
    usbdrv_device_t apple_plain;
    usbdrv_device_t android;
    usbdrv_device_t stick;
    usbmod_device_t view;
    char error[USBMOD_ERR_LEN];
    expect(mod != NULL, "create");
    usbmod_set_switch_aoa(mod, 0);

    apple_plain = device_of("2-4", 0x05ac, 0x12a8, NULL);
    expect(usbmod_handle_add(mod, &apple_plain) == 0, "add plain apple");
    expect(usbmod_find(mod, "usb-2-4-05ac-12a8", &view) == 0, "plain id");
    expect(view.airplay_ready == 0, "no transport");
    expect(usbmod_start(mod, view.device_id, USBMOD_MODE_AIRPLAY, error, sizeof error) != 0, "airplay rejected");
    expect(strstr(error, "NCM") != NULL, "reason names the class");

    apple = device_of("2-1", 0x05ac, 0x12a8, "SN");
    apple.class_ncm = 1;
    snprintf(apple.net_iface, sizeof apple.net_iface, "usb0");
    expect(usbmod_handle_add(mod, &apple) == 0, "add apple");
    expect(usbmod_find(mod, "usb-05ac-12a8-SN", &view) == 0, "apple id");
    expect(view.kind == USBMOD_KIND_APPLE && view.airplay_ready == 1, "airplay ready");
    expect(view.state == USBMOD_STATE_READY, "apple ready");
    expect(usbmod_start(mod, view.device_id, USBMOD_MODE_AIRPLAY, error, sizeof error) == 0, "start airplay");
    expect(usbmod_find(mod, view.device_id, &view) == 0, "reload apple");
    expect(view.state == USBMOD_STATE_ACTIVE && view.mode == USBMOD_MODE_AIRPLAY, "airplay active");
    expect(strstr(view.reason, "usb0") != NULL, "iface in reason");

    android = device_of("2-3", 0x18d1, 0x4ee7, "AND");
    android.class_adb = 1;
    expect(usbmod_classify(&android) == USBMOD_KIND_ANDROID, "adb is android");
    expect(usbmod_handle_add(mod, &android) == 0, "add android");
    expect(usbmod_find(mod, "usb-18d1-4ee7-AND", &view) == 0, "android id");
    expect(view.state == USBMOD_STATE_PROBING, "probe deferred");
    expect(usbmod_start(mod, view.device_id, USBMOD_MODE_AOA, error, sizeof error) != 0, "aoa before probe");
    expect(usbmod_finish_probe(mod, view.device_id, 2) == 0, "protocol 2");
    expect(usbmod_start(mod, view.device_id, USBMOD_MODE_AOA, error, sizeof error) != 0, "exclusive blocks aoa");
    expect(usbmod_stop(mod, "usb-05ac-12a8-SN", error, sizeof error) == 0, "stop airplay");
    expect(usbmod_start(mod, "usb-18d1-4ee7-AND", USBMOD_MODE_AOA, error, sizeof error) == 0, "start aoa");

    stick = device_of("2-2", 0x0781, 0x5581, NULL);
    stick.class_storage = 1;
    snprintf(stick.block_dev, sizeof stick.block_dev, "sda");
    expect(usbmod_handle_add(mod, &stick) == 0, "add stick");
    expect(usbmod_find(mod, "usb-2-2-0781-5581", &view) == 0, "stick id");
    expect(view.kind == USBMOD_KIND_STORAGE, "storage kind");
    expect(usbmod_start(mod, view.device_id, USBMOD_MODE_STORAGE, error, sizeof error) == 0, "storage with aoa active");

    expect(usbmod_handle_remove(mod, "2-2") == 0, "unplug stick");
    expect(usbmod_find(mod, "usb-2-2-0781-5581", &view) != 0, "stick gone");
    expect(usbmod_count(mod) == 3, "three devices remain");
    usbmod_destroy(mod);
}

static void test_modes(void)
{
    usbmod_mode_t mode;
    expect(usbmod_mode_from_str("airplay", &mode) == 0 && mode == USBMOD_MODE_AIRPLAY, "parse airplay");
    expect(usbmod_mode_from_str("aoa", &mode) == 0 && mode == USBMOD_MODE_AOA, "parse aoa");
    expect(strcmp(usbmod_kind_str(USBMOD_KIND_APPLE), "apple") == 0, "kind string");
}

int main(void)
{
    test_aoa_plan();
    test_sessions();
    test_modes();
    if (g_failed) {
        fprintf(stderr, "usbmod-selftest failed\n");
        return 1;
    }
    printf("usbmod-selftest ok\n");
    return 0;
}
