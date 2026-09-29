#ifndef USB_MGR_ORCHESTRATOR_H
#define USB_MGR_ORCHESTRATOR_H

#include "adapter.h"
#include "policy.h"
#include "registry.h"

typedef void (*usb_session_change_cb)(const usb_session_t* session, void* user);

typedef struct usb_orchestrator usb_orchestrator_t;

usb_orchestrator_t* usb_orchestrator_create(usb_registry_t* registry, usb_policy_t* policy);
void usb_orchestrator_destroy(usb_orchestrator_t* orch);
void usb_orchestrator_set_adapters(usb_orchestrator_t* orch, usb_adapter_t* android_auto,
                                   usb_adapter_t* carplay);
void usb_orchestrator_set_session_callback(usb_orchestrator_t* orch, usb_session_change_cb cb,
                                           void* user);

void usb_orchestrator_on_added(usb_orchestrator_t* orch, const usb_device_t* device);
void usb_orchestrator_on_removed(usb_orchestrator_t* orch, const char* device_id);
int usb_orchestrator_start(usb_orchestrator_t* orch, const char* device_id, usb_session_mode_t mode,
                           char* error_out, size_t error_len);
int usb_orchestrator_stop(usb_orchestrator_t* orch, const char* device_id, char* error_out,
                          size_t error_len);
int usb_orchestrator_active(const usb_orchestrator_t* orch, usb_session_t* out);

#endif
