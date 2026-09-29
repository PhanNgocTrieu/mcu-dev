#include "classifier.h"
#include "manager.h"
#include "policy.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_failures = 0;
static int g_mounts = 0;
static int g_unmounts = 0;
static int g_projections = 0;
static char g_projection_mode[32];
static char g_last_session_reason[USB_ERR_LEN];

static void expect(int cond, const char* msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_failures++;
    } else {
        printf("OK: %s\n", msg);
    }
}

static int fake_find(const char* sys_path, char* block_out, size_t block_len)
{
    (void)sys_path;
    snprintf(block_out, block_len, "%s", "/dev/fake-sda1");
    return 1;
}

static int fake_mount(const char* block, const char* target, char* err, size_t err_len)
{
    (void)block;
    (void)target;
    g_mounts++;
    snprintf(err, err_len, "%s", "mounted_fake");
    return 1;
}

static int fake_unmount(const char* target, char* err, size_t err_len)
{
    (void)target;
    g_unmounts++;
    snprintf(err, err_len, "%s", "unmounted_fake");
    return 1;
}

static void on_projection(const char* device_id, const char* mode, void* user)
{
    (void)device_id;
    (void)user;
    g_projections++;
    snprintf(g_projection_mode, sizeof(g_projection_mode), "%s", mode);
}

static void on_session(const usb_session_t* session, void* user)
{
    (void)user;
    snprintf(g_last_session_reason, sizeof(g_last_session_reason), "%s", session->reason);
}

static void fill_phone(usb_device_t* device, uint16_t vid, const char* serial, const char* sys_path)
{
    memset(device, 0, sizeof(*device));
    device->vendor_id = vid;
    device->product_id = 0x4ee1;
    snprintf(device->serial, sizeof(device->serial), "%s", serial);
    snprintf(device->sys_path, sizeof(device->sys_path), "%s", sys_path);
    snprintf(device->manufacturer, sizeof(device->manufacturer), "%s", vid == 0x05ac ? "Apple" : "Google");
    snprintf(device->product, sizeof(device->product), "%s", vid == 0x05ac ? "iPhone" : "Pixel");
}

static void test_classifier(void)
{
    usb_device_t device;

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x05ac;
    expect(usb_classify(&device) == USB_TYPE_IPHONE, "Apple VID -> iPhone");

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x18d1;
    expect(usb_classify(&device) == USB_TYPE_ANDROID, "Google VID -> Android");

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x04e8;
    snprintf(device.interfaces, sizeof(device.interfaces), "%s", ":ff4201:");
    expect(usb_classify(&device) == USB_TYPE_ANDROID, "ADB interface -> Android");

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x1234;
    snprintf(device.interfaces, sizeof(device.interfaces), "%s", ":ff0000:");
    expect(usb_classify(&device) == USB_TYPE_UNKNOWN, "vendor class FF alone is not Android");

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x0781;
    snprintf(device.interfaces, sizeof(device.interfaces), "%s", ":080650:");
    expect(usb_classify(&device) == USB_TYPE_MASS_STORAGE, "class 08 -> mass storage");

    memset(&device, 0, sizeof(device));
    device.vendor_id = 0x046d;
    snprintf(device.interfaces, sizeof(device.interfaces), "%s", ":030101:");
    expect(usb_classify(&device) == USB_TYPE_HID, "class 03 -> HID");
}

static void test_policy(void)
{
    usb_policy_t* policy = usb_policy_create();
    usb_device_t android;
    usb_device_t iphone;
    usb_policy_decision_t decision;
    usb_session_t active;

    fill_phone(&android, 0x18d1, "ABC", "/sys/a");
    android.type = USB_TYPE_ANDROID;
    android.state = USB_STATE_READY;
    usb_assign_id(&android);

    fill_phone(&iphone, 0x05ac, "XYZ", "/sys/b");
    iphone.type = USB_TYPE_IPHONE;
    iphone.state = USB_STATE_READY;
    usb_assign_id(&iphone);

    usb_policy_can_start(policy, &android, USB_MODE_ANDROID_AUTO, NULL, &decision);
    expect(decision.allowed, "AA start allowed when idle");

    memset(&active, 0, sizeof(active));
    snprintf(active.device_id, sizeof(active.device_id), "%s", android.device_id);
    active.mode = USB_MODE_ANDROID_AUTO;
    active.state = USB_SESSION_ACTIVE;
    usb_policy_can_start(policy, &iphone, USB_MODE_CARPLAY, &active, &decision);
    expect(!decision.allowed, "exclusive: reject CarPlay while AA active");
    expect(strstr(decision.reason, "exclusive") != NULL, "exclusive reason string");

    usb_policy_can_start(policy, &android, USB_MODE_STORAGE, NULL, &decision);
    expect(!decision.allowed, "storage is not started through StartSession");

    usb_policy_set_allowlist(policy, 1);
    usb_policy_allow_serial(policy, "ABC");
    usb_policy_can_start(policy, &android, USB_MODE_ANDROID_AUTO, NULL, &decision);
    expect(decision.allowed, "allowlist permits ABC");
    usb_policy_can_start(policy, &iphone, USB_MODE_CARPLAY, NULL, &decision);
    expect(!decision.allowed, "allowlist rejects XYZ");
    usb_policy_destroy(policy);
}

static void test_manager(void)
{
    usb_storage_ops_t ops;
    usb_registry_t* registry;
    usb_policy_t* policy;
    usb_manager_t* manager;
    usb_adapter_t android_auto;
    usb_adapter_t carplay;
    usb_manager_listener_t listener;
    usb_device_t phone;
    usb_device_t iphone;
    usb_device_t stick;
    usb_device_t changed;
    usb_device_t hid;
    usb_device_t found;
    usb_session_t active;
    char err[USB_ERR_LEN];
    char root[] = "/tmp/cm-usb-selftest";

    memset(&ops, 0, sizeof(ops));
    ops.find_block = fake_find;
    ops.mount_fs = fake_mount;
    ops.unmount_fs = fake_unmount;
    usb_storage_set_root(root);
    usb_storage_set_ops(&ops);

    registry = usb_registry_create();
    policy = usb_policy_create();
    manager = usb_manager_create(registry, policy);
    usb_adapter_android_init(&android_auto);
    usb_adapter_carplay_init(&carplay);
    usb_manager_set_adapters(manager, &android_auto, &carplay);
    memset(&listener, 0, sizeof(listener));
    listener.on_projection = on_projection;
    listener.on_session = on_session;
    usb_manager_set_listener(manager, &listener);

    fill_phone(&phone, 0x18d1, "PIXEL", "/sys/devices/usb/1-1.2");
    g_projections = 0;
    usb_manager_on_added(manager, &phone);
    expect(usb_registry_lookup(registry, &phone, &found), "android registered");
    expect(found.state == USB_STATE_READY, "android stops at Ready");
    expect(g_projections == 1 && strcmp(g_projection_mode, "android_auto") == 0,
           "ProjectionAvailable for Android Auto service");
    expect(!usb_manager_active(manager, &active), "AA service has not claimed a session yet");

    fill_phone(&iphone, 0x05ac, "IPHONE", "/sys/devices/usb/1-1.3");
    usb_manager_on_added(manager, &iphone);
    expect(usb_registry_lookup(registry, &iphone, &found), "iphone registered");
    expect(found.state == USB_STATE_READY, "iPhone stays Ready");
    expect(strcmp(g_projection_mode, "carplay") == 0, "ProjectionAvailable for CarPlay service");
    expect(!usb_manager_start_session(manager, found.device_id, USB_MODE_CARPLAY, err, sizeof(err)),
           "CarPlay start returns MFi error");
    expect(strcmp(err, "MFI_REQUIRED") == 0, "MFI_REQUIRED reason");
    expect(usb_registry_get(registry, found.device_id, &found) && found.state == USB_STATE_READY,
           "iPhone remains Ready after stub rejection");
    expect(!usb_manager_active(manager, &active), "failed CarPlay does not occupy the session");

    expect(usb_registry_lookup(registry, &phone, &found), "android still present");
    expect(usb_manager_start_session(manager, found.device_id, USB_MODE_ANDROID_AUTO, err, sizeof(err)),
           "AA service claims the phone");
    expect(usb_manager_active(manager, &active), "one active projection");
    expect(usb_registry_lookup(registry, &iphone, &found), "lookup iphone id");
    expect(!usb_manager_start_session(manager, found.device_id, USB_MODE_CARPLAY, err, sizeof(err)),
           "second projection rejected");
    expect(strstr(err, "exclusive") != NULL, "exclusive while AA is claimed");

    memset(&stick, 0, sizeof(stick));
    stick.vendor_id = 0x0781;
    stick.product_id = 0x5567;
    snprintf(stick.serial, sizeof(stick.serial), "%s", "STICK");
    snprintf(stick.sys_path, sizeof(stick.sys_path), "%s", "/sys/devices/usb/1-1.4");
    snprintf(stick.interfaces, sizeof(stick.interfaces), "%s", ":080650:");
    snprintf(stick.product, sizeof(stick.product), "%s", "USB Disk");
    g_mounts = 0;
    usb_manager_on_added(manager, &stick);
    expect(usb_registry_lookup(registry, &stick, &found), "stick registered");
    expect(found.state == USB_STATE_READY, "stick mounted to Ready");
    expect(found.mount_point[0] != '\0', "mount point published");
    expect(g_mounts == 1, "mount called once");
    expect(usb_manager_active(manager, &active) && active.mode == USB_MODE_ANDROID_AUTO,
           "storage runs beside the AA session");

    changed = stick;
    snprintf(changed.product, sizeof(changed.product), "%s", "USB Disk renamed");
    usb_manager_on_changed(manager, &changed);
    expect(g_mounts == 1, "change does not mount again");

    memset(&hid, 0, sizeof(hid));
    hid.vendor_id = 0x046d;
    snprintf(hid.sys_path, sizeof(hid.sys_path), "%s", "/sys/devices/usb/1-1.5");
    snprintf(hid.interfaces, sizeof(hid.interfaces), "%s", ":030101:");
    usb_manager_on_added(manager, &hid);
    expect(usb_registry_lookup(registry, &hid, &found) && found.state == USB_STATE_IGNORED, "HID ignored");

    usb_manager_on_removed(manager, &phone);
    expect(!usb_manager_active(manager, &active), "unplug releases the AA session");
    expect(!usb_registry_lookup(registry, &phone, &found), "android removed");

    usb_manager_on_removed(manager, &stick);
    expect(g_unmounts == 1, "unplug unmounts storage");
    expect(!usb_registry_lookup(registry, &stick, &found), "stick removed");

    usb_adapter_shutdown(&android_auto);
    usb_adapter_shutdown(&carplay);
    usb_manager_destroy(manager);
    usb_policy_destroy(policy);
    usb_registry_destroy(registry);
    rmdir(root);
}

int main(void)
{
    test_classifier();
    test_policy();
    test_manager();
    if (g_failures) {
        fprintf(stderr, "%d failures\n", g_failures);
        return 1;
    }
    printf("All checks passed\n");
    return 0;
}
