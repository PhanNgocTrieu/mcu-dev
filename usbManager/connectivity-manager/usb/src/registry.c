#include "registry.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct usb_registry {
    pthread_mutex_t mu;
    usb_device_t devices[USB_MAX_DEVICES];
    int count;
};

usb_registry_t* usb_registry_create(void)
{
    usb_registry_t* registry = calloc(1, sizeof(*registry));
    if (!registry) {
        return NULL;
    }
    pthread_mutex_init(&registry->mu, NULL);
    return registry;
}

void usb_registry_destroy(usb_registry_t* registry)
{
    if (!registry) {
        return;
    }
    pthread_mutex_destroy(&registry->mu);
    free(registry);
}

static int find_index(usb_registry_t* registry, const char* device_id)
{
    int i;

    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->devices[i].device_id, device_id) == 0) {
            return i;
        }
    }
    return -1;
}

static int lookup_index(usb_registry_t* registry, const usb_device_t* key)
{
    int i;

    if (key->sys_path[0]) {
        for (i = 0; i < registry->count; ++i) {
            if (strcmp(registry->devices[i].sys_path, key->sys_path) == 0) {
                return i;
            }
        }
    }
    if (key->device_id[0]) {
        int idx = find_index(registry, key->device_id);
        if (idx >= 0) {
            return idx;
        }
    }
    if (key->serial[0]) {
        for (i = 0; i < registry->count; ++i) {
            if (registry->devices[i].vendor_id == key->vendor_id &&
                registry->devices[i].product_id == key->product_id &&
                strcmp(registry->devices[i].serial, key->serial) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void usb_registry_upsert(usb_registry_t* registry, const usb_device_t* device)
{
    int idx;

    if (!registry || !device || !device->device_id[0]) {
        return;
    }
    pthread_mutex_lock(&registry->mu);
    idx = find_index(registry, device->device_id);
    if (idx >= 0) {
        registry->devices[idx] = *device;
    } else if (registry->count < USB_MAX_DEVICES) {
        registry->devices[registry->count++] = *device;
    }
    pthread_mutex_unlock(&registry->mu);
}

int usb_registry_remove(usb_registry_t* registry, const char* device_id)
{
    int idx;
    int found = 0;

    if (!registry || !device_id) {
        return 0;
    }
    pthread_mutex_lock(&registry->mu);
    idx = find_index(registry, device_id);
    if (idx >= 0) {
        memmove(&registry->devices[idx], &registry->devices[idx + 1],
                (size_t)(registry->count - idx - 1) * sizeof(usb_device_t));
        registry->count--;
        found = 1;
    }
    pthread_mutex_unlock(&registry->mu);
    return found;
}

int usb_registry_get(usb_registry_t* registry, const char* device_id, usb_device_t* out)
{
    int idx;
    int found = 0;

    if (!registry || !device_id || !out) {
        return 0;
    }
    pthread_mutex_lock(&registry->mu);
    idx = find_index(registry, device_id);
    if (idx >= 0) {
        *out = registry->devices[idx];
        found = 1;
    }
    pthread_mutex_unlock(&registry->mu);
    return found;
}

int usb_registry_list(usb_registry_t* registry, usb_device_t* out, int max)
{
    int count;

    if (!registry || !out || max <= 0) {
        return 0;
    }
    pthread_mutex_lock(&registry->mu);
    count = registry->count < max ? registry->count : max;
    memcpy(out, registry->devices, (size_t)count * sizeof(usb_device_t));
    pthread_mutex_unlock(&registry->mu);
    return count;
}

int usb_registry_lookup(usb_registry_t* registry, const usb_device_t* key, usb_device_t* out)
{
    int idx;
    int found = 0;

    if (!registry || !key || !out) {
        return 0;
    }
    pthread_mutex_lock(&registry->mu);
    idx = lookup_index(registry, key);
    if (idx >= 0) {
        *out = registry->devices[idx];
        found = 1;
    }
    pthread_mutex_unlock(&registry->mu);
    return found;
}
