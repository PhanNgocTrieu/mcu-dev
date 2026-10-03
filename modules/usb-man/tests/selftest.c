#include "usbman.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static usbman_dev_t dev_android(void)
{
    usbman_dev_t d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "sim-android");
    d.vid = 0x18d1;
    d.pid = 0x4ee2;
    d.adb = 1;
    d.sim = 1;
    return d;
}

static usbman_dev_t dev_aoap(void)
{
    usbman_dev_t d = dev_android();
    d.pid = 0x2d00;
    d.adb = 0;
    d.accessory = 1;
    return d;
}

static usbman_dev_t dev_carplay(void)
{
    usbman_dev_t d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "sim-carplay");
    snprintf(d.net, sizeof d.net, "usb0");
    d.vid = 0x05ac;
    d.pid = 0x12a8;
    d.apple = 1;
    d.ncm = 1;
    d.sim = 1;
    return d;
}

static void test_classify(void)
{
    usbman_dev_t d = dev_android();
    expect(usbman_classify(&d) == USBMAN_KIND_ANDROID, "android");
    d = dev_aoap();
    expect(usbman_classify(&d) == USBMAN_KIND_ANDROID_AOAP, "aoap");
    d = dev_carplay();
    expect(usbman_classify(&d) == USBMAN_KIND_CARPLAY, "carplay");
    d.ncm = 0;
    d.ipheth = 1;
    expect(usbman_classify(&d) == USBMAN_KIND_APPLE_IPHETH, "ipheth blocked");
    d.ipheth = 0;
    expect(usbman_classify(&d) == USBMAN_KIND_APPLE_WAIT, "wait ncm");
}

static void test_one_backend(void)
{
    usbman_session_t s;
    usbman_dev_t android = dev_android();
    usbman_dev_t carplay = dev_carplay();
    usbman_dev_t aoap = dev_aoap();
    char line[512];
    char lines[9][USBMAN_LINE_LEN];
    int n;

    usbman_session_init(&s);
    expect(usbman_on_device(&s, &android) == USBMAN_ACT_AOA, "start aoa");
    expect(strcmp(s.backend, "android") == 0, "backend android");
    expect(strcmp(s.phase, "aoa") == 0, "phase aoa");
    expect(usbman_on_device(&s, &carplay) == USBMAN_ACT_NONE, "second phone waits");
    expect(strcmp(s.parked, "sim-carplay") == 0, "parked carplay");
    expect(strcmp(s.backend, "android") == 0, "backend stays");

    usbman_expect_reenum(&s);
    usbman_on_gone(&s, "sim-android");
    expect(strcmp(s.phase, "reenumerating") == 0, "reenum");
    expect(strcmp(s.backend, "android") == 0, "backend held");
    expect(usbman_on_device(&s, &aoap) == USBMAN_ACT_NONE, "aoap completes");
    expect(strcmp(s.phase, "active") == 0, "active");
    expect(s.streaming == 1, "stream on");

    usbman_set_stream(&s, 0);
    expect(s.streaming == 0, "stream forced off");
    usbman_set_stream(&s, 1);
    expect(s.streaming == 1, "stream forced on");

    usbman_touch(&s, 5000, 2500, 1);
    usbman_state_line(&s, line, sizeof line);
    expect(strstr(line, "backend=android") != NULL, "state backend");
    expect(strstr(line, "touchq=1") != NULL, "touch queued");
    expect(strstr(line, "touch=5000,2500,1") != NULL, "touch point");

    usbman_on_gone(&s, "sim-android");
    expect(strcmp(s.phase, "idle") == 0, "unplug idle");
    expect(strcmp(s.backend, "none") == 0, "backend clear");

    expect(usbman_on_device(&s, &carplay) == USBMAN_ACT_NONE, "sim carplay skips ip");
    expect(strcmp(s.backend, "carplay") == 0 && strcmp(s.phase, "active") == 0, "carplay active");
    expect(s.streaming == 1, "carplay streams");

    n = usbman_aoa_build("sim-android", lines, 9);
    expect(n == 9, "aoa steps");
    expect(strncmp(lines[0], "claim sim-android", 17) == 0, "claim");
    expect(strstr(lines[1], " 51 ") != NULL, "get protocol");
    expect(strstr(lines[8], " 53 ") != NULL, "start");
}

static void test_parse_roundtrip_fields(void)
{
    const char *line =
        "dev id=sim-carplay vid=0x05ac pid=0x12a8 serial=SIM-CARPLAY mfg=Apple prod=iPhone "
        "ifaces=02/0d/00 drivers=cdc_ncm net=usb0 node=- ncm=1 ecm=0 rndis=0 ipheth=0 "
        "storage=0 hid=0 adb=0 accessory=0 sim=1 apple=1";
    usbman_dev_t d;
    expect(usbman_parse_dev(line, &d) == 0, "parse");
    expect(d.vid == 0x05ac && d.ncm == 1 && d.apple == 1, "flags");
    expect(usbman_classify(&d) == USBMAN_KIND_CARPLAY, "parsed carplay");
    expect(usbman_parse_dev("dev gone id=sim-carplay", &d) != 0, "gone is not a device");
}

int main(void)
{
    test_classify();
    test_one_backend();
    test_parse_roundtrip_fields();
    if (g_fail) {
        fprintf(stderr, "usb-manager selftest failed\n");
        return 1;
    }
    printf("usb-manager selftest ok\n");
    return 0;
}
