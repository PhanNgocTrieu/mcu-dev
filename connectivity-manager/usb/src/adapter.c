#include "adapter.h"

#include "log.h"
#include "transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int claimed;
    char device_id[USB_ID_LEN];
} usb_claim_t;

static int aa_probe(usb_device_t* device, char* msg, size_t msg_len, void* ctx)
{
    usb_aoap_result_t aoap;
    usb_ncm_result_t ncm;

    (void)ctx;
    usb_probe_aoap(device, &aoap);
    usb_probe_ncm(device, &ncm);
    device->aoap_supported = aoap.supported;
    device->ncm_present = ncm.iface_present;
    snprintf(device->net_iface, sizeof(device->net_iface), "%s", ncm.if_name);
    /*
     * Probe chỉ ghi nhận transport. Điện thoại vẫn Ready để service Android Auto
     * tự nối, kể cả khi AOAP/NCM chưa lên (điện thoại có thể còn ở MTP).
     */
    snprintf(msg, msg_len, "aoap=%s; ncm=%s", aoap.detail, ncm.detail);
    return device->type == USB_TYPE_ANDROID;
}

static int aa_start(const usb_device_t* device, char* msg, size_t msg_len, void* ctx)
{
    usb_claim_t* claim = ctx;

    if (!claim) {
        snprintf(msg, msg_len, "%s", "no_claim");
        return 0;
    }
    if (claim->claimed) {
        snprintf(msg, msg_len, "%s", "already_active");
        return 0;
    }
    claim->claimed = 1;
    snprintf(claim->device_id, sizeof(claim->device_id), "%s", device->device_id);
    snprintf(msg, msg_len, "%s", "aa_claimed_for_external_service");
    USB_LOG_INFO("Android Auto session claimed; protocol stays in the AA service");
    return 1;
}

static int aa_stop(const usb_device_t* device, void* ctx)
{
    usb_claim_t* claim = ctx;

    (void)device;
    if (!claim || !claim->claimed) {
        return 1;
    }
    USB_LOG_INFO("Android Auto session released");
    claim->claimed = 0;
    claim->device_id[0] = '\0';
    return 1;
}

static int cp_probe(usb_device_t* device, char* msg, size_t msg_len, void* ctx)
{
    (void)ctx;
    if (device->type != USB_TYPE_IPHONE && device->vendor_id != 0x05ac) {
        snprintf(msg, msg_len, "%s", "not_apple_device");
        return 0;
    }
    /* Điện thoại vẫn Ready. Service CarPlay gọi StartSession và nhận MFI_REQUIRED. */
    snprintf(msg, msg_len, "%s", "apple_present_mfi_required");
    USB_LOG_INFO("CarPlay candidate ready, MFi not available in this prototype");
    return 1;
}

static int cp_start(const usb_device_t* device, char* msg, size_t msg_len, void* ctx)
{
    (void)device;
    (void)ctx;
    USB_LOG_WARN("CarPlay start rejected: MFI_REQUIRED");
    snprintf(msg, msg_len, "%s", "MFI_REQUIRED");
    return 0;
}

static int cp_stop(const usb_device_t* device, void* ctx)
{
    (void)device;
    (void)ctx;
    return 1;
}

void usb_adapter_android_init(usb_adapter_t* adapter)
{
    usb_claim_t* claim;

    memset(adapter, 0, sizeof(*adapter));
    claim = calloc(1, sizeof(*claim));
    adapter->name = "AndroidAutoClaim";
    adapter->probe = aa_probe;
    adapter->start = aa_start;
    adapter->stop = aa_stop;
    adapter->ctx = claim;
}

void usb_adapter_carplay_init(usb_adapter_t* adapter)
{
    memset(adapter, 0, sizeof(*adapter));
    adapter->name = "CarPlayStub";
    adapter->probe = cp_probe;
    adapter->start = cp_start;
    adapter->stop = cp_stop;
    adapter->ctx = NULL;
}

void usb_adapter_shutdown(usb_adapter_t* adapter)
{
    if (!adapter) {
        return;
    }
    free(adapter->ctx);
    adapter->ctx = NULL;
}
