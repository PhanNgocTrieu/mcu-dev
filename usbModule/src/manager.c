#include "usbmod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBMOD_MAX 32

typedef struct {
    usbdrv_device_t raw;
    usbmod_device_t view;
    int used;
} usbmod_slot_t;

struct usbmod {
    usbmod_slot_t slots[USBMOD_MAX];
    int switch_aoa;
    usbmod_aoa_strings_t aoa;
    char aoa_manufacturer[USBDRV_STR_LEN];
    char aoa_model[USBDRV_STR_LEN];
    char aoa_description[USBDRV_STR_LEN];
    char aoa_version[USBDRV_STR_LEN];
    char aoa_uri[USBDRV_STR_LEN];
    char aoa_serial[USBDRV_STR_LEN];
    char active_id[USBMOD_ID_LEN];
    usbmod_listener_t listener;
};

static void copy_str(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (cap == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void set_err(char *error, size_t error_len, const char *text)
{
    if (error != NULL && error_len > 0) {
        snprintf(error, error_len, "%s", text != NULL ? text : "");
    }
}

static int projection_mode(usbmod_mode_t mode)
{
    return mode == USBMOD_MODE_AOA || mode == USBMOD_MODE_AIRPLAY;
}

static void assign_id(usbmod_device_t *view, const usbdrv_device_t *raw)
{
    size_t i;
    size_t o = 0;
    if (raw->serial[0] == '\0') {
        snprintf(view->device_id, sizeof view->device_id, "usb-%s-%04x-%04x", raw->sys_name, raw->vendor_id,
                 raw->product_id);
        return;
    }
    {
        int wrote = snprintf(view->device_id, sizeof view->device_id, "usb-%04x-%04x-", raw->vendor_id,
                             raw->product_id);
        if (wrote < 0 || (size_t)wrote >= sizeof view->device_id) {
            return;
        }
        o = (size_t)wrote;
    }
    for (i = 0; raw->serial[i] != '\0' && o + 1 < sizeof view->device_id; i++) {
        char c = raw->serial[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            view->device_id[o++] = c;
        }
    }
    view->device_id[o] = '\0';
}

static void fill_view(usbmod_slot_t *slot)
{
    usbmod_device_t *view = &slot->view;
    const usbdrv_device_t *raw = &slot->raw;
    char id[USBMOD_ID_LEN];
    char reason[USBMOD_ERR_LEN];
    usbmod_state_t state = view->state;
    usbmod_mode_t mode = view->mode;
    int protocol = view->aoa_protocol;
    copy_str(id, sizeof id, view->device_id);
    copy_str(reason, sizeof reason, view->reason);
    memset(view, 0, sizeof *view);
    if (id[0] == '\0') {
        assign_id(view, raw);
    } else {
        copy_str(view->device_id, sizeof view->device_id, id);
    }
    copy_str(view->sys_name, sizeof view->sys_name, raw->sys_name);
    view->vendor_id = raw->vendor_id;
    view->product_id = raw->product_id;
    copy_str(view->serial, sizeof view->serial, raw->serial);
    view->kind = usbmod_classify(raw);
    view->state = state;
    view->mode = mode;
    view->aoa_protocol = protocol;
    view->airplay_ready = usbmod_airplay_ready(raw);
    copy_str(view->net_iface, sizeof view->net_iface, raw->net_iface);
    copy_str(view->devnode, sizeof view->devnode, raw->devnode);
    copy_str(view->block_dev, sizeof view->block_dev, raw->block_dev);
    copy_str(view->reason, sizeof view->reason, reason);
}

static void emit(usbmod_t *mod, const usbmod_device_t *view, const char *event)
{
    if (mod->listener.on_device != NULL) {
        mod->listener.on_device(view, event, mod->listener.user);
    }
}

static usbmod_slot_t *find_slot_name(usbmod_t *mod, const char *sys_name)
{
    size_t i;
    for (i = 0; i < USBMOD_MAX; i++) {
        if (mod->slots[i].used && strcmp(mod->slots[i].view.sys_name, sys_name) == 0) {
            return &mod->slots[i];
        }
    }
    return NULL;
}

static usbmod_slot_t *find_slot_id(usbmod_t *mod, const char *device_id)
{
    size_t i;
    for (i = 0; i < USBMOD_MAX; i++) {
        if (mod->slots[i].used && strcmp(mod->slots[i].view.device_id, device_id) == 0) {
            return &mod->slots[i];
        }
    }
    return NULL;
}

static usbmod_slot_t *alloc_slot(usbmod_t *mod)
{
    size_t i;
    for (i = 0; i < USBMOD_MAX; i++) {
        if (!mod->slots[i].used) {
            memset(&mod->slots[i], 0, sizeof mod->slots[i]);
            mod->slots[i].used = 1;
            mod->slots[i].view.aoa_protocol = -1;
            return &mod->slots[i];
        }
    }
    return NULL;
}

static void settle(usbmod_t *mod, usbmod_slot_t *slot, int fresh)
{
    usbmod_device_t *view = &slot->view;
    char reason[USBMOD_ERR_LEN];
    int protocol = -1;
    copy_str(reason, sizeof reason, "");
    if (!fresh && view->state == USBMOD_STATE_ACTIVE) {
        fill_view(slot);
        copy_str(view->reason, sizeof view->reason, "session active");
        return;
    }
    view->state = USBMOD_STATE_CLASSIFIED;
    view->mode = USBMOD_MODE_NONE;
    view->aoa_protocol = -1;
    fill_view(slot);

    if (view->kind == USBMOD_KIND_ANDROID) {
        if (slot->raw.class_aoap) {
            view->state = USBMOD_STATE_READY;
            view->aoa_protocol = 1;
            copy_str(view->reason, sizeof view->reason, "already in accessory mode");
            return;
        }
        view->state = USBMOD_STATE_PROBING;
        copy_str(view->reason, sizeof view->reason, "AOA probe deferred");
        if (mod->switch_aoa && view->devnode[0] != '\0') {
            int rc = usbmod_aoa_run(view->devnode, &mod->aoa, &protocol, reason, sizeof reason);
            if (rc == 0) {
                view->aoa_protocol = protocol;
                view->state = USBMOD_STATE_READY;
                copy_str(view->reason, sizeof view->reason, "AOA start sent, waiting re-enumeration");
            } else if (protocol == 0) {
                view->aoa_protocol = 0;
                view->state = USBMOD_STATE_READY;
                copy_str(view->reason, sizeof view->reason, reason);
            } else if (protocol > 0) {
                view->aoa_protocol = protocol;
                view->state = USBMOD_STATE_READY;
                copy_str(view->reason, sizeof view->reason, reason);
            } else {
                view->state = USBMOD_STATE_PROBING;
                copy_str(view->reason, sizeof view->reason, reason[0] ? reason : "AOA probe did not run");
            }
        }
        return;
    }
    if (view->kind == USBMOD_KIND_APPLE) {
        view->state = USBMOD_STATE_READY;
        if (view->airplay_ready) {
            snprintf(view->reason, sizeof view->reason, "AirPlay transport ready on %s",
                     view->net_iface[0] ? view->net_iface : "class driver");
        } else {
            copy_str(view->reason, sizeof view->reason, "Apple device enumerated");
        }
        return;
    }
    if (view->kind == USBMOD_KIND_STORAGE) {
        view->state = USBMOD_STATE_READY;
        copy_str(view->reason, sizeof view->reason, "mass storage enumerated");
        return;
    }
    if (view->kind == USBMOD_KIND_HID) {
        view->state = USBMOD_STATE_READY;
        copy_str(view->reason, sizeof view->reason, "HID enumerated");
        return;
    }
    if (view->kind == USBMOD_KIND_NETWORK) {
        view->state = USBMOD_STATE_READY;
        copy_str(view->reason, sizeof view->reason, "network class enumerated");
        return;
    }
    if (slot->raw.class_acm) {
        view->state = USBMOD_STATE_READY;
        copy_str(view->reason, sizeof view->reason, "CDC-ACM enumerated");
        return;
    }
    view->state = USBMOD_STATE_IGNORED;
    copy_str(view->reason, sizeof view->reason, "no supported class");
}

usbmod_t *usbmod_create(void)
{
    usbmod_t *mod = calloc(1, sizeof *mod);
    if (mod == NULL) {
        return NULL;
    }
    copy_str(mod->aoa_manufacturer, sizeof mod->aoa_manufacturer, "MCU");
    copy_str(mod->aoa_model, sizeof mod->aoa_model, "UsbModule");
    copy_str(mod->aoa_description, sizeof mod->aoa_description, "Android Open Accessory");
    copy_str(mod->aoa_version, sizeof mod->aoa_version, "1.0");
    copy_str(mod->aoa_uri, sizeof mod->aoa_uri, "http://localhost/usb");
    copy_str(mod->aoa_serial, sizeof mod->aoa_serial, "usb-module");
    mod->aoa.manufacturer = mod->aoa_manufacturer;
    mod->aoa.model = mod->aoa_model;
    mod->aoa.description = mod->aoa_description;
    mod->aoa.version = mod->aoa_version;
    mod->aoa.uri = mod->aoa_uri;
    mod->aoa.serial = mod->aoa_serial;
    return mod;
}

void usbmod_destroy(usbmod_t *mod)
{
    free(mod);
}

void usbmod_set_listener(usbmod_t *mod, const usbmod_listener_t *listener)
{
    if (mod == NULL) {
        return;
    }
    if (listener == NULL) {
        memset(&mod->listener, 0, sizeof mod->listener);
        return;
    }
    mod->listener = *listener;
}

void usbmod_set_switch_aoa(usbmod_t *mod, int enabled)
{
    if (mod != NULL) {
        mod->switch_aoa = enabled ? 1 : 0;
    }
}

void usbmod_set_aoa_strings(usbmod_t *mod, const usbmod_aoa_strings_t *strings)
{
    if (mod == NULL || strings == NULL) {
        return;
    }
    if (strings->manufacturer != NULL) {
        copy_str(mod->aoa_manufacturer, sizeof mod->aoa_manufacturer, strings->manufacturer);
    }
    if (strings->model != NULL) {
        copy_str(mod->aoa_model, sizeof mod->aoa_model, strings->model);
    }
    if (strings->description != NULL) {
        copy_str(mod->aoa_description, sizeof mod->aoa_description, strings->description);
    }
    if (strings->version != NULL) {
        copy_str(mod->aoa_version, sizeof mod->aoa_version, strings->version);
    }
    if (strings->uri != NULL) {
        copy_str(mod->aoa_uri, sizeof mod->aoa_uri, strings->uri);
    }
    if (strings->serial != NULL) {
        copy_str(mod->aoa_serial, sizeof mod->aoa_serial, strings->serial);
    }
}

int usbmod_handle_add(usbmod_t *mod, const usbdrv_device_t *device)
{
    usbmod_slot_t *slot;
    int fresh;
    if (mod == NULL || device == NULL || device->sys_name[0] == '\0') {
        return -1;
    }
    slot = find_slot_name(mod, device->sys_name);
    fresh = slot == NULL;
    if (fresh) {
        slot = alloc_slot(mod);
        if (slot == NULL) {
            return -1;
        }
        slot->view.aoa_protocol = -1;
        slot->raw = *device;
        settle(mod, slot, 1);
        emit(mod, &slot->view, "added");
        return 0;
    }
    slot->raw = *device;
    fill_view(slot);
    if (slot->view.state != USBMOD_STATE_ACTIVE && slot->view.kind == USBMOD_KIND_ANDROID &&
        slot->raw.class_aoap) {
        slot->view.aoa_protocol = slot->view.aoa_protocol < 0 ? 1 : slot->view.aoa_protocol;
        slot->view.state = USBMOD_STATE_READY;
        copy_str(slot->view.reason, sizeof slot->view.reason, "already in accessory mode");
    } else if (slot->view.state == USBMOD_STATE_READY && slot->view.kind == USBMOD_KIND_APPLE &&
               slot->view.airplay_ready) {
        snprintf(slot->view.reason, sizeof slot->view.reason, "AirPlay transport ready on %s",
                 slot->view.net_iface[0] ? slot->view.net_iface : "class driver");
    }
    emit(mod, &slot->view, "updated");
    return 0;
}

int usbmod_handle_remove(usbmod_t *mod, const char *sys_name)
{
    usbmod_slot_t *slot;
    usbmod_device_t copy;
    if (mod == NULL || sys_name == NULL) {
        return -1;
    }
    slot = find_slot_name(mod, sys_name);
    if (slot == NULL) {
        return -1;
    }
    copy = slot->view;
    copy.state = USBMOD_STATE_IGNORED;
    if (strcmp(mod->active_id, copy.device_id) == 0) {
        mod->active_id[0] = '\0';
    }
    slot->used = 0;
    emit(mod, &copy, "removed");
    return 0;
}

int usbmod_scan(usbmod_t *mod, const char *sysfs_devices)
{
    usbdrv_device_t found[USBMOD_MAX];
    size_t count = 0;
    size_t i;
    int seen[USBMOD_MAX];
    if (mod == NULL) {
        return -1;
    }
    memset(seen, 0, sizeof seen);
    if (usbdrv_enum_root(sysfs_devices, found, USBMOD_MAX, &count) != 0) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        usbmod_slot_t *slot;
        usbmod_handle_add(mod, &found[i]);
        slot = find_slot_name(mod, found[i].sys_name);
        if (slot != NULL) {
            seen[(size_t)(slot - mod->slots)] = 1;
        }
    }
    for (i = 0; i < USBMOD_MAX; i++) {
        if (mod->slots[i].used && !seen[i]) {
            usbmod_handle_remove(mod, mod->slots[i].view.sys_name);
        }
    }
    return 0;
}

int usbmod_finish_probe(usbmod_t *mod, const char *device_id, int protocol)
{
    usbmod_slot_t *slot;
    if (mod == NULL || device_id == NULL) {
        return -1;
    }
    slot = find_slot_id(mod, device_id);
    if (slot == NULL || slot->view.kind != USBMOD_KIND_ANDROID) {
        return -1;
    }
    if (slot->view.state == USBMOD_STATE_ACTIVE) {
        return -1;
    }
    slot->view.aoa_protocol = protocol;
    slot->view.state = USBMOD_STATE_READY;
    slot->view.mode = USBMOD_MODE_NONE;
    if (protocol >= 1) {
        snprintf(slot->view.reason, sizeof slot->view.reason, "AOA protocol %d", protocol);
    } else {
        copy_str(slot->view.reason, sizeof slot->view.reason, "AOA protocol is not supported");
    }
    emit(mod, &slot->view, "updated");
    return 0;
}

int usbmod_start(usbmod_t *mod, const char *device_id, usbmod_mode_t mode, char *error, size_t error_len)
{
    usbmod_slot_t *slot;
    if (mod == NULL || device_id == NULL) {
        set_err(error, error_len, "missing device");
        return -1;
    }
    slot = find_slot_id(mod, device_id);
    if (slot == NULL) {
        set_err(error, error_len, "device is not enumerated");
        return -1;
    }
    if (projection_mode(mode) && mod->active_id[0] != '\0' && strcmp(mod->active_id, device_id) != 0) {
        set_err(error, error_len, "another projection session is active");
        return -1;
    }
    if (mode == USBMOD_MODE_AIRPLAY) {
        if (slot->view.kind != USBMOD_KIND_APPLE) {
            set_err(error, error_len, "AirPlay needs an Apple device");
            return -1;
        }
        if (!slot->view.airplay_ready) {
            slot->view.state = USBMOD_STATE_FAILED;
            copy_str(slot->view.reason, sizeof slot->view.reason,
                     "AirPlay needs NCM, ECM, RNDIS, or ipheth");
            set_err(error, error_len, slot->view.reason);
            emit(mod, &slot->view, "session");
            return -1;
        }
        slot->view.state = USBMOD_STATE_ACTIVE;
        slot->view.mode = USBMOD_MODE_AIRPLAY;
        snprintf(slot->view.reason, sizeof slot->view.reason, "airplay-transport-ready:%s",
                 slot->view.net_iface[0] ? slot->view.net_iface : "class");
        copy_str(mod->active_id, sizeof mod->active_id, slot->view.device_id);
        set_err(error, error_len, slot->view.reason);
        emit(mod, &slot->view, "session");
        return 0;
    }
    if (mode == USBMOD_MODE_AOA) {
        if (slot->view.kind != USBMOD_KIND_ANDROID) {
            set_err(error, error_len, "AOA needs an Android device");
            return -1;
        }
        if (slot->view.aoa_protocol == 0) {
            slot->view.state = USBMOD_STATE_FAILED;
            copy_str(slot->view.reason, sizeof slot->view.reason, "AOA protocol is not supported");
            set_err(error, error_len, slot->view.reason);
            emit(mod, &slot->view, "session");
            return -1;
        }
        if (slot->view.aoa_protocol < 0 && !slot->raw.class_aoap) {
            set_err(error, error_len, "AOA probe is not finished");
            return -1;
        }
        slot->view.state = USBMOD_STATE_ACTIVE;
        slot->view.mode = USBMOD_MODE_AOA;
        copy_str(slot->view.reason, sizeof slot->view.reason, "aoa-session-ready");
        copy_str(mod->active_id, sizeof mod->active_id, slot->view.device_id);
        set_err(error, error_len, slot->view.reason);
        emit(mod, &slot->view, "session");
        return 0;
    }
    if (mode == USBMOD_MODE_STORAGE) {
        if (slot->view.kind != USBMOD_KIND_STORAGE) {
            set_err(error, error_len, "device is not mass storage");
            return -1;
        }
        slot->view.state = USBMOD_STATE_ACTIVE;
        slot->view.mode = USBMOD_MODE_STORAGE;
        copy_str(slot->view.reason, sizeof slot->view.reason, "storage-ready");
        set_err(error, error_len, slot->view.reason);
        emit(mod, &slot->view, "session");
        return 0;
    }
    set_err(error, error_len, "unsupported mode");
    return -1;
}

int usbmod_stop(usbmod_t *mod, const char *device_id, char *error, size_t error_len)
{
    usbmod_slot_t *slot;
    if (mod == NULL || device_id == NULL) {
        set_err(error, error_len, "missing device");
        return -1;
    }
    slot = find_slot_id(mod, device_id);
    if (slot == NULL) {
        set_err(error, error_len, "device is not enumerated");
        return -1;
    }
    if (strcmp(mod->active_id, device_id) == 0) {
        mod->active_id[0] = '\0';
    }
    slot->view.mode = USBMOD_MODE_NONE;
    if (slot->view.state == USBMOD_STATE_ACTIVE || slot->view.state == USBMOD_STATE_FAILED) {
        slot->view.state = USBMOD_STATE_READY;
    }
    copy_str(slot->view.reason, sizeof slot->view.reason, "session stopped");
    set_err(error, error_len, slot->view.reason);
    emit(mod, &slot->view, "session");
    return 0;
}

size_t usbmod_count(const usbmod_t *mod)
{
    size_t i;
    size_t n = 0;
    if (mod == NULL) {
        return 0;
    }
    for (i = 0; i < USBMOD_MAX; i++) {
        if (mod->slots[i].used) {
            n++;
        }
    }
    return n;
}

int usbmod_get(const usbmod_t *mod, size_t index, usbmod_device_t *out)
{
    size_t i;
    size_t seen = 0;
    if (mod == NULL || out == NULL) {
        return -1;
    }
    for (i = 0; i < USBMOD_MAX; i++) {
        if (!mod->slots[i].used) {
            continue;
        }
        if (seen == index) {
            *out = mod->slots[i].view;
            return 0;
        }
        seen++;
    }
    return -1;
}

int usbmod_find(const usbmod_t *mod, const char *device_id, usbmod_device_t *out)
{
    size_t i;
    if (mod == NULL || device_id == NULL || out == NULL) {
        return -1;
    }
    for (i = 0; i < USBMOD_MAX; i++) {
        if (mod->slots[i].used && strcmp(mod->slots[i].view.device_id, device_id) == 0) {
            *out = mod->slots[i].view;
            return 0;
        }
    }
    return -1;
}
