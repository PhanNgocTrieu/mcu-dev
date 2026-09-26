#include "policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct usb_policy {
  int allowlist_enabled;
  char serials[USB_MAX_ALLOW][USB_STR_LEN];
  int serial_count;
};

usb_policy_t* usb_policy_create(void) { return calloc(1, sizeof(usb_policy_t)); }

void usb_policy_destroy(usb_policy_t* policy) { free(policy); }

void usb_policy_set_allowlist(usb_policy_t* policy, int enabled) {
  if (policy) policy->allowlist_enabled = enabled;
}

void usb_policy_allow_serial(usb_policy_t* policy, const char* serial) {
  if (!policy || !serial || !serial[0] || policy->serial_count >= USB_MAX_ALLOW) return;
  strncpy(policy->serials[policy->serial_count], serial, USB_STR_LEN - 1);
  policy->serials[policy->serial_count][USB_STR_LEN - 1] = '\0';
  policy->serial_count++;
}

usb_session_mode_t usb_policy_preferred_mode(const usb_device_t* device) {
  if (!device) return USB_MODE_NONE;
  switch (device->type) {
    case USB_TYPE_ANDROID: return USB_MODE_ANDROID_AUTO;
    case USB_TYPE_IPHONE: return USB_MODE_CARPLAY;
    case USB_TYPE_MASS_STORAGE: return USB_MODE_STORAGE;
    default: return USB_MODE_NONE;
  }
}

static int serial_allowed(const usb_policy_t* policy, const char* serial) {
  if (!serial || !serial[0]) return 0;
  for (int i = 0; i < policy->serial_count; ++i) {
    if (strcmp(policy->serials[i], serial) == 0) return 1;
  }
  return 0;
}

static void set_decision(usb_policy_decision_t* out, int allowed, const char* reason) {
  out->allowed = allowed;
  strncpy(out->reason, reason ? reason : "", USB_ERR_LEN - 1);
  out->reason[USB_ERR_LEN - 1] = '\0';
}

void usb_policy_can_start(const usb_policy_t* policy, const usb_device_t* device,
                          usb_session_mode_t mode, const usb_session_t* active, usb_policy_decision_t* out) {
  if (!out) return;
  if (!policy || !device || mode == USB_MODE_NONE) {
    set_decision(out, 0, "invalid_mode");
    return;
  }
  if (device->state == USB_STATE_IGNORED || device->state == USB_STATE_FAILED) {
    set_decision(out, 0, "device_not_ready");
    return;
  }
  if (policy->allowlist_enabled && !serial_allowed(policy, device->serial)) {
    set_decision(out, 0, "not_in_allowlist");
    return;
  }
  if ((mode == USB_MODE_ANDROID_AUTO || mode == USB_MODE_CARPLAY) && active &&
      active->state == USB_SESSION_ACTIVE &&
      (active->mode == USB_MODE_ANDROID_AUTO || active->mode == USB_MODE_CARPLAY) &&
      strcmp(active->device_id, device->device_id) != 0) {
    char reason[USB_ERR_LEN];
    snprintf(reason, sizeof(reason), "exclusive_session_active:%s", active->device_id);
    set_decision(out, 0, reason);
    return;
  }
  if (mode == USB_MODE_ANDROID_AUTO && device->type != USB_TYPE_ANDROID) {
    set_decision(out, 0, "type_mismatch_android");
    return;
  }
  if (mode == USB_MODE_CARPLAY && device->type != USB_TYPE_IPHONE) {
    set_decision(out, 0, "type_mismatch_iphone");
    return;
  }
  if (mode == USB_MODE_STORAGE && device->type != USB_TYPE_MASS_STORAGE) {
    set_decision(out, 0, "type_mismatch_storage");
    return;
  }
  set_decision(out, 1, "ok");
}
