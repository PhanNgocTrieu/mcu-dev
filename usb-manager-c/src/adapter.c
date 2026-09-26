#include "adapter.h"

#include "log.h"
#include "transport.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	int active;
	char device_id[USB_ID_LEN];
} usb_demo_session_t;

static int demo_start(const usb_device_t* device, usb_demo_session_t* session, char* msg, size_t msg_len) {
	if (session->active) {
		snprintf(msg, msg_len, "%s", "already_active");
		return 0;
  	}
  	session->active = 1;
  	snprintf(session->device_id, sizeof(session->device_id), "%s", device->device_id);
 	USB_LOG_INFO("AASDK demo: session start");
  	USB_LOG_INFO("AASDK demo: VideoChannel / AudioChannel / InputChannel / SensorChannel");
	snprintf(msg, msg_len, "%s", "aa_session_active:DemoAasdkSession");
	return 1;
}

static void demo_stop(usb_demo_session_t* session) {
  if (!session->active) return;
  USB_LOG_INFO("AASDK demo: session stop");
  session->active = 0;
  session->device_id[0] = '\0';
}

static int aa_probe(usb_device_t* device, char* msg, size_t msg_len, void* ctx) {
  (void)ctx;
  usb_aoap_result_t aoap;
  usb_ncm_result_t ncm;
  usb_probe_aoap(device, &aoap);
  usb_probe_ncm(device, &ncm);
  device->aoap_supported = aoap.supported;
  device->ncm_present = ncm.iface_present;
  snprintf(device->net_iface, sizeof(device->net_iface), "%s", ncm.if_name);
  int ok = aoap.supported || ncm.iface_present;
  snprintf(msg, msg_len, "aoap=%s; ncm=%s", aoap.detail, ncm.detail);
  if (!ok && device->type == USB_TYPE_ANDROID) {
    ok = 1;
    snprintf(msg, msg_len, "aoap=%s; ncm=%s; proceed_demo_without_transport", aoap.detail, ncm.detail);
    USB_LOG_WARN("AA probe: no AOAP/NCM yet — demo path");
  }
  return ok;
}

static int aa_start(const usb_device_t* device, char* msg, size_t msg_len, void* ctx) {
  return demo_start(device, ctx, msg, msg_len);
}

static int aa_stop(const usb_device_t* device, void* ctx) {
  (void)device;
  demo_stop(ctx);
  return 1;
}

static int cp_probe(usb_device_t* device, char* msg, size_t msg_len, void* ctx) {
  (void)ctx;
  if (device->type != USB_TYPE_IPHONE && device->vendor_id != 0x05ac) {
    snprintf(msg, msg_len, "%s", "not_apple_device");
    return 0;
  }
  USB_LOG_INFO("CarPlay stub: Available=false Reason=MFI_REQUIRED");
  snprintf(msg, msg_len, "%s", "detected_apple_stub_only");
  return 1;
}

static int cp_start(const usb_device_t* device, char* msg, size_t msg_len, void* ctx) {
  (void)device;
  (void)ctx;
  USB_LOG_WARN("CarPlay stub: start rejected — MFI_REQUIRED");
  snprintf(msg, msg_len, "%s", "MFI_REQUIRED");
  return 0;
}

static int cp_stop(const usb_device_t* device, void* ctx) {
  (void)device;
  (void)ctx;
  return 1;
}

void usb_adapter_android_init(usb_adapter_t* adapter) {
  static usb_demo_session_t session;
  memset(&session, 0, sizeof(session));
  adapter->name = "AndroidAutoAdapter/DemoAasdkSession";
  adapter->probe = aa_probe;
  adapter->start = aa_start;
  adapter->stop = aa_stop;
  adapter->ctx = &session;
}

void usb_adapter_carplay_init(usb_adapter_t* adapter) {
  adapter->name = "CarPlayStubAdapter";
  adapter->probe = cp_probe;
  adapter->start = cp_start;
  adapter->stop = cp_stop;
  adapter->ctx = NULL;
}
