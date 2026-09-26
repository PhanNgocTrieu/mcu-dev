#include "orchestrator.h"

#include "log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct usb_orchestrator {
  usb_registry_t* registry;
  usb_policy_t* policy;
  usb_adapter_t* aa;
  usb_adapter_t* cp;
  usb_session_t active;
  usb_session_change_cb on_session;
  void* user;
  pthread_mutex_t mu;
};

static void copy_msg(char* dst, size_t n, const char* src) {
  if (!dst || n == 0) return;
  snprintf(dst, n, "%s", src ? src : "");
}

static void emit_session(usb_orchestrator_t* orch, const usb_session_t* info) {
  orch->active = *info;
  if (orch->on_session) orch->on_session(info, orch->user);
}

static void transition(usb_orchestrator_t* orch, usb_device_t* device, usb_device_state_t next,
                       const char* reason) {
  char msg[256];
  snprintf(msg, sizeof(msg), "state %s %s -> %s (%s)", device->device_id, usb_state_str(device->state),
           usb_state_str(next), reason ? reason : "");
  USB_LOG_INFO(msg);
  device->state = next;
  usb_registry_upsert(orch->registry, device, reason);
}

static usb_adapter_t* adapter_for(usb_orchestrator_t* orch, usb_session_mode_t mode) {
  if (mode == USB_MODE_ANDROID_AUTO) return orch->aa;
  if (mode == USB_MODE_CARPLAY) return orch->cp;
  return NULL;
}

usb_orchestrator_t* usb_orchestrator_create(usb_registry_t* registry, usb_policy_t* policy) {
  usb_orchestrator_t* orch = calloc(1, sizeof(*orch));
  if (!orch) return NULL;
  orch->registry = registry;
  orch->policy = policy;
  orch->active.state = USB_SESSION_IDLE;
  orch->active.mode = USB_MODE_NONE;
  pthread_mutex_init(&orch->mu, NULL);
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_destroy(&orch->mu);
  pthread_mutex_init(&orch->mu, &attr);
  pthread_mutexattr_destroy(&attr);
  return orch;
}

void usb_orchestrator_destroy(usb_orchestrator_t* orch) {
  if (!orch) return;
  pthread_mutex_destroy(&orch->mu);
  free(orch);
}

void usb_orchestrator_set_adapters(usb_orchestrator_t* orch, usb_adapter_t* android_auto,
                                   usb_adapter_t* carplay) {
  if (!orch) return;
  orch->aa = android_auto;
  orch->cp = carplay;
}

void usb_orchestrator_set_session_callback(usb_orchestrator_t* orch, usb_session_change_cb cb, void* user) {
  if (!orch) return;
  orch->on_session = cb;
  orch->user = user;
}

int usb_orchestrator_active(const usb_orchestrator_t* orch, usb_session_t* out) {
  if (!orch || !out) return 0;
  pthread_mutex_lock((pthread_mutex_t*)&orch->mu);
  int ok = !(orch->active.state == USB_SESSION_IDLE || orch->active.mode == USB_MODE_NONE);
  if (ok) *out = orch->active;
  pthread_mutex_unlock((pthread_mutex_t*)&orch->mu);
  return ok;
}

static int start_unlocked(usb_orchestrator_t* orch, const char* device_id, usb_session_mode_t mode,
                          char* error_out, size_t error_len) {
  usb_device_t device;
  if (!usb_registry_get(orch->registry, device_id, &device)) {
    copy_msg(error_out, error_len, "device_not_found");
    return 0;
  }
  usb_session_t active_copy = orch->active;
  const usb_session_t* active_ptr =
      (active_copy.state == USB_SESSION_IDLE || active_copy.mode == USB_MODE_NONE) ? NULL : &active_copy;
  usb_policy_decision_t decision;
  usb_policy_can_start(orch->policy, &device, mode, active_ptr, &decision);
  if (!decision.allowed) {
    copy_msg(error_out, error_len, decision.reason);
    USB_LOG_WARN(decision.reason);
    return 0;
  }
  usb_adapter_t* adapter = adapter_for(orch, mode);
  if (!adapter && mode != USB_MODE_STORAGE) {
    copy_msg(error_out, error_len, "no_adapter");
    return 0;
  }
  transition(orch, &device, USB_STATE_CONNECTING, "session_start");
  usb_session_t starting;
  memset(&starting, 0, sizeof(starting));
  snprintf(starting.device_id, sizeof(starting.device_id), "%s", device_id);
  starting.mode = mode;
  starting.state = USB_SESSION_STARTING;
  copy_msg(starting.reason, sizeof(starting.reason), "starting");
  emit_session(orch, &starting);

  if (mode == USB_MODE_STORAGE) {
    transition(orch, &device, USB_STATE_ACTIVE, "storage_mounted_stub");
    usb_session_t s = starting;
    s.state = USB_SESSION_ACTIVE;
    copy_msg(s.reason, sizeof(s.reason), "storage_ok");
    emit_session(orch, &s);
    return 1;
  }

  char msg[USB_ERR_LEN];
  int ok = adapter->start(&device, msg, sizeof(msg), adapter->ctx);
  usb_device_t refreshed;
  if (usb_registry_get(orch->registry, device_id, &refreshed)) device = refreshed;
  if (!ok) {
    copy_msg(error_out, error_len, msg);
    transition(orch, &device, USB_STATE_FAILED, msg);
    usb_session_t failed = starting;
    failed.state = USB_SESSION_FAILED;
    copy_msg(failed.reason, sizeof(failed.reason), msg);
    emit_session(orch, &failed);
    return 0;
  }
  transition(orch, &device, USB_STATE_ACTIVE, msg);
  usb_session_t active = starting;
  active.state = USB_SESSION_ACTIVE;
  copy_msg(active.reason, sizeof(active.reason), msg);
  emit_session(orch, &active);
  return 1;
}

static void probe_fields(usb_device_t* device, void* ctx) {
  usb_device_t* src = ctx;
  device->aoap_supported = src->aoap_supported;
  device->ncm_present = src->ncm_present;
  snprintf(device->net_iface, sizeof(device->net_iface), "%s", src->net_iface);
  snprintf(device->last_error, sizeof(device->last_error), "%s", src->last_error);
}

static void probe_unlocked(usb_orchestrator_t* orch, const char* device_id) {
  usb_device_t device;
  if (!usb_registry_get(orch->registry, device_id, &device)) return;
  transition(orch, &device, USB_STATE_PROBING, "probe_start");
  usb_session_mode_t mode = usb_policy_preferred_mode(&device);
  usb_adapter_t* adapter = adapter_for(orch, mode);
  if (!adapter) {
    if (device.type == USB_TYPE_MASS_STORAGE) transition(orch, &device, USB_STATE_READY, "storage_ready");
    else transition(orch, &device, USB_STATE_IGNORED, "no_adapter");
    return;
  }
  char msg[USB_ERR_LEN];
  int ok = adapter->probe(&device, msg, sizeof(msg), adapter->ctx);
  copy_msg(device.last_error, sizeof(device.last_error), ok ? "" : msg);
  usb_registry_update(orch->registry, device_id, probe_fields, &device, "probe_fields");
  if (!usb_registry_get(orch->registry, device_id, &device)) return;
  if (!ok) {
    copy_msg(device.last_error, sizeof(device.last_error), msg);
    transition(orch, &device, USB_STATE_FAILED, msg);
    return;
  }
  transition(orch, &device, USB_STATE_READY, msg);
  if (mode == USB_MODE_ANDROID_AUTO) {
    char err[USB_ERR_LEN];
    start_unlocked(orch, device_id, USB_MODE_ANDROID_AUTO, err, sizeof(err));
  } else if (mode == USB_MODE_CARPLAY) {
    usb_session_t s;
    memset(&s, 0, sizeof(s));
    snprintf(s.device_id, sizeof(s.device_id), "%s", device_id);
    s.mode = USB_MODE_CARPLAY;
    s.state = USB_SESSION_FAILED;
    copy_msg(s.reason, sizeof(s.reason), "MFI_REQUIRED");
    emit_session(orch, &s);
    USB_LOG_INFO("CarPlay available=false");
  }
}

void usb_orchestrator_on_added(usb_orchestrator_t* orch, const usb_device_t* incoming) {
  if (!orch || !incoming) return;
  pthread_mutex_lock(&orch->mu);
  usb_device_t device = *incoming;
  device.state = USB_STATE_ENUMERATING;
  usb_registry_upsert(orch->registry, &device, "add");
  if (device.type == USB_TYPE_UNKNOWN || device.type == USB_TYPE_HID) {
    device.state = USB_STATE_IGNORED;
    usb_registry_upsert(orch->registry, &device, "ignored");
    pthread_mutex_unlock(&orch->mu);
    return;
  }
  transition(orch, &device, USB_STATE_CLASSIFIED, usb_type_str(device.type));
  probe_unlocked(orch, device.device_id);
  pthread_mutex_unlock(&orch->mu);
}

void usb_orchestrator_on_removed(usb_orchestrator_t* orch, const char* device_id) {
  if (!orch || !device_id) return;
  pthread_mutex_lock(&orch->mu);
  usb_device_t existing;
  if (!usb_registry_get(orch->registry, device_id, &existing)) {
    pthread_mutex_unlock(&orch->mu);
    return;
  }
  if (strcmp(orch->active.device_id, device_id) == 0 &&
      (orch->active.state == USB_SESSION_ACTIVE || orch->active.state == USB_SESSION_STARTING)) {
    char err[USB_ERR_LEN];
    usb_orchestrator_stop(orch, device_id, err, sizeof(err));
  }
  if (usb_registry_get(orch->registry, device_id, &existing)) {
    transition(orch, &existing, USB_STATE_DISCONNECTING, "udev_remove");
    usb_registry_remove(orch->registry, device_id, "removed");
  }
  pthread_mutex_unlock(&orch->mu);
}

int usb_orchestrator_start(usb_orchestrator_t* orch, const char* device_id, usb_session_mode_t mode,
                           char* error_out, size_t error_len) {
  if (!orch) return 0;
  pthread_mutex_lock(&orch->mu);
  int ok = start_unlocked(orch, device_id, mode, error_out, error_len);
  pthread_mutex_unlock(&orch->mu);
  return ok;
}

int usb_orchestrator_stop(usb_orchestrator_t* orch, const char* device_id, char* error_out, size_t error_len) {
  if (!orch) return 0;
  pthread_mutex_lock(&orch->mu);
  usb_device_t device;
  if (!usb_registry_get(orch->registry, device_id, &device)) {
    copy_msg(error_out, error_len, "device_not_found");
    pthread_mutex_unlock(&orch->mu);
    return 0;
  }
  usb_session_mode_t mode = (strcmp(orch->active.device_id, device_id) == 0)
                                ? orch->active.mode
                                : usb_policy_preferred_mode(&device);
  usb_adapter_t* adapter = adapter_for(orch, mode);
  usb_session_t stopping;
  memset(&stopping, 0, sizeof(stopping));
  snprintf(stopping.device_id, sizeof(stopping.device_id), "%s", device_id);
  stopping.mode = mode;
  stopping.state = USB_SESSION_STOPPING;
  copy_msg(stopping.reason, sizeof(stopping.reason), "stopping");
  emit_session(orch, &stopping);
  if (adapter && adapter->stop) adapter->stop(&device, adapter->ctx);
  if (usb_registry_get(orch->registry, device_id, &device)) {
    transition(orch, &device, USB_STATE_READY, "session_stopped");
  }
  usb_session_t idle;
  memset(&idle, 0, sizeof(idle));
  idle.mode = USB_MODE_NONE;
  idle.state = USB_SESSION_IDLE;
  copy_msg(idle.reason, sizeof(idle.reason), "stopped");
  emit_session(orch, &idle);
  pthread_mutex_unlock(&orch->mu);
  return 1;
}
