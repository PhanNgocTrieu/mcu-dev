#ifndef CM_USB_POLICY_H
#define CM_USB_POLICY_H

#include "types.h"

#define USB_MAX_ALLOW 16

typedef struct {
    int allowed;
    char reason[USB_ERR_LEN];
} usb_policy_decision_t;

typedef struct usb_policy usb_policy_t;

usb_policy_t* usb_policy_create(void);
void usb_policy_destroy(usb_policy_t* policy);
void usb_policy_set_allowlist(usb_policy_t* policy, int enabled);
void usb_policy_allow_serial(usb_policy_t* policy, const char* serial);

/* Android -> android_auto, iPhone -> carplay, ổ đĩa -> storage. */
usb_session_mode_t usb_policy_preferred_mode(const usb_device_t* device);

/*
 * Chỉ một phiên Android Auto hoặc CarPlay. Ổ đĩa không đi qua đây:
 * USB manager tự mount và cho phép ổ đĩa song song với projection.
 */
void usb_policy_can_start(const usb_policy_t* policy, const usb_device_t* device,
                          usb_session_mode_t mode, const usb_session_t* active_or_null,
                          usb_policy_decision_t* out);

#endif
