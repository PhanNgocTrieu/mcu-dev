#include "registry.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct usb_registry {
  pthread_mutex_t mu;
  usb_device_t devices[USB_MAX_DEVICES];
  int count;
  usb_device_change_cb on_change;
  void* user;
};

usb_registry_t* usb_registry_create(void) {
  usb_registry_t* reg = calloc(1, sizeof(*reg));
  if (!reg) return NULL;
  pthread_mutex_init(&reg->mu, NULL);
  return reg;
}

void usb_registry_destroy(usb_registry_t* reg) {
  if (!reg) return;
  pthread_mutex_destroy(&reg->mu);
  free(reg);
}

void usb_registry_set_callback(usb_registry_t* reg, usb_device_change_cb cb, void* user) {
  if (!reg) return;
  pthread_mutex_lock(&reg->mu);
  reg->on_change = cb;
  reg->user = user;
  pthread_mutex_unlock(&reg->mu);
}

static int find_index(usb_registry_t* reg, const char* device_id) {
  for (int i = 0; i < reg->count; ++i) {
    if (strcmp(reg->devices[i].device_id, device_id) == 0) return i;
  }
  return -1;
}

void usb_registry_upsert(usb_registry_t* reg, const usb_device_t* device, const char* reason) {
  if (!reg || !device) return;
  usb_device_t copy = *device;
  usb_device_change_cb cb = NULL;
  void* user = NULL;
  pthread_mutex_lock(&reg->mu);
  int idx = find_index(reg, device->device_id);
  if (idx >= 0) {
    reg->devices[idx] = *device;
  } else if (reg->count < USB_MAX_DEVICES) {
    reg->devices[reg->count++] = *device;
  }
  cb = reg->on_change;
  user = reg->user;
  pthread_mutex_unlock(&reg->mu);
  if (cb) cb(&copy, reason ? reason : "update", user);
}

int usb_registry_remove(usb_registry_t* reg, const char* device_id, const char* reason) {
  if (!reg || !device_id) return 0;
  usb_device_t copy;
  usb_device_change_cb cb = NULL;
  void* user = NULL;
  int found = 0;
  pthread_mutex_lock(&reg->mu);
  int idx = find_index(reg, device_id);
  if (idx >= 0) {
    copy = reg->devices[idx];
    copy.state = USB_STATE_IDLE;
    memmove(&reg->devices[idx], &reg->devices[idx + 1],
            (size_t)(reg->count - idx - 1) * sizeof(usb_device_t));
    reg->count--;
    found = 1;
    cb = reg->on_change;
    user = reg->user;
  }
  pthread_mutex_unlock(&reg->mu);
  if (found && cb) cb(&copy, reason ? reason : "remove", user);
  return found;
}

int usb_registry_get(usb_registry_t* reg, const char* device_id, usb_device_t* out) {
  if (!reg || !device_id || !out) return 0;
  int found = 0;
  pthread_mutex_lock(&reg->mu);
  int idx = find_index(reg, device_id);
  if (idx >= 0) {
    *out = reg->devices[idx];
    found = 1;
  }
  pthread_mutex_unlock(&reg->mu);
  return found;
}

int usb_registry_list(usb_registry_t* reg, usb_device_t* out, int max) {
  if (!reg || !out || max <= 0) return 0;
  pthread_mutex_lock(&reg->mu);
  int n = reg->count < max ? reg->count : max;
  memcpy(out, reg->devices, (size_t)n * sizeof(usb_device_t));
  pthread_mutex_unlock(&reg->mu);
  return n;
}

int usb_registry_update(usb_registry_t* reg, const char* device_id,
                        void (*mutator)(usb_device_t* device, void* ctx), void* ctx, const char* reason) {
  if (!reg || !device_id || !mutator) return 0;
  usb_device_t copy;
  usb_device_change_cb cb = NULL;
  void* user = NULL;
  int found = 0;
  pthread_mutex_lock(&reg->mu);
  int idx = find_index(reg, device_id);
  if (idx >= 0) {
    mutator(&reg->devices[idx], ctx);
    copy = reg->devices[idx];
    found = 1;
    cb = reg->on_change;
    user = reg->user;
  }
  pthread_mutex_unlock(&reg->mu);
  if (found && cb) cb(&copy, reason ? reason : "update", user);
  return found;
}
