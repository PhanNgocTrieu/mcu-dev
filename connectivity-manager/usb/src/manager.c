#include "manager.h"

#include "classifier.h"
#include "log.h"
#include "storage.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * epoch tăng mỗi lần unplug hoặc StopSession hủy phiên đang mở.
 * StartSession đang chạy dở thấy epoch đổi thì không ghi đè trạng thái Active.
 * adapter_mu tách khỏi mu để probe/mount và unplug không kẹt nhau trong libusb.
 */

struct usb_manager {
    usb_registry_t* registry;
    usb_policy_t* policy;
    usb_adapter_t* android_auto;
    usb_adapter_t* carplay;
    usb_session_t active;
    usb_manager_listener_t listener;
    pthread_mutex_t mu;
    pthread_mutex_t adapter_mu;
    unsigned epoch;
};

static void copy_msg(char* dst, size_t len, const char* src)
{
    if (!dst || len == 0) {
        return;
    }
    snprintf(dst, len, "%s", src ? src : "");
}

static void emit_device(usb_manager_t* manager, const usb_device_t* device, const char* reason)
{
    if (manager->listener.on_device) {
        manager->listener.on_device(device, reason, manager->listener.user);
    }
}

static void emit_session(usb_manager_t* manager, const usb_session_t* session)
{
    if (manager->listener.on_session) {
        manager->listener.on_session(session, manager->listener.user);
    }
}

static void emit_projection(usb_manager_t* manager, const char* device_id, const char* mode)
{
    if (manager->listener.on_projection) {
        manager->listener.on_projection(device_id, mode, manager->listener.user);
    }
}

static void save_device(usb_manager_t* manager, const usb_device_t* device, const char* reason)
{
    usb_registry_upsert(manager->registry, device);
    emit_device(manager, device, reason);
}

static usb_adapter_t* adapter_for(usb_manager_t* manager, usb_session_mode_t mode)
{
    if (mode == USB_MODE_ANDROID_AUTO) {
        return manager->android_auto;
    }
    if (mode == USB_MODE_CARPLAY) {
        return manager->carplay;
    }
    return NULL;
}

static void clear_session(usb_session_t* session, const char* reason)
{
    memset(session, 0, sizeof(*session));
    session->mode = USB_MODE_NONE;
    session->state = USB_SESSION_IDLE;
    copy_msg(session->reason, sizeof(session->reason), reason);
}

static int still_present(usb_manager_t* manager, const char* device_id, usb_device_t* out);
void usb_manager_on_changed(usb_manager_t* manager, const usb_device_t* device);

static int still_present(usb_manager_t* manager, const char* device_id, usb_device_t* out)
{
    return usb_registry_get(manager->registry, device_id, out);
}

usb_manager_t* usb_manager_create(usb_registry_t* registry, usb_policy_t* policy)
{
    usb_manager_t* manager = calloc(1, sizeof(*manager));

    if (!manager) {
        return NULL;
    }
    manager->registry = registry;
    manager->policy = policy;
    manager->active.state = USB_SESSION_IDLE;
    manager->active.mode = USB_MODE_NONE;
    pthread_mutex_init(&manager->mu, NULL);
    pthread_mutex_init(&manager->adapter_mu, NULL);
    return manager;
}

void usb_manager_destroy(usb_manager_t* manager)
{
    if (!manager) {
        return;
    }
    pthread_mutex_destroy(&manager->adapter_mu);
    pthread_mutex_destroy(&manager->mu);
    free(manager);
}

void usb_manager_set_adapters(usb_manager_t* manager, usb_adapter_t* android_auto, usb_adapter_t* carplay)
{
    if (!manager) {
        return;
    }
    pthread_mutex_lock(&manager->mu);
    manager->android_auto = android_auto;
    manager->carplay = carplay;
    pthread_mutex_unlock(&manager->mu);
}

void usb_manager_set_listener(usb_manager_t* manager, const usb_manager_listener_t* listener)
{
    if (!manager) {
        return;
    }
    pthread_mutex_lock(&manager->mu);
    if (listener) {
        manager->listener = *listener;
    } else {
        memset(&manager->listener, 0, sizeof(manager->listener));
    }
    pthread_mutex_unlock(&manager->mu);
}

static void finish_storage(usb_manager_t* manager, usb_device_t* device, int mounted, const char* detail)
{
    usb_device_t stored;

    pthread_mutex_lock(&manager->mu);
    if (!still_present(manager, device->device_id, &stored)) {
        pthread_mutex_unlock(&manager->mu);
        if (mounted) {
            char err[USB_ERR_LEN];
            usb_storage_detach(device->mount_point, err, sizeof(err));
        }
        return;
    }
    snprintf(stored.block_dev, sizeof(stored.block_dev), "%s", device->block_dev);
    snprintf(stored.mount_point, sizeof(stored.mount_point), "%s", mounted ? device->mount_point : "");
    copy_msg(stored.last_error, sizeof(stored.last_error), detail);
    stored.state = mounted ? USB_STATE_READY : USB_STATE_FAILED;
    save_device(manager, &stored, mounted ? "storage_mounted" : "storage_mount_failed");
    pthread_mutex_unlock(&manager->mu);
}

static void finish_phone(usb_manager_t* manager, const usb_device_t* probed, int ok, const char* detail,
                         int announce)
{
    usb_device_t stored;
    char device_id[USB_ID_LEN];
    char mode[32];
    int notify = 0;

    pthread_mutex_lock(&manager->mu);
    if (!still_present(manager, probed->device_id, &stored)) {
        pthread_mutex_unlock(&manager->mu);
        return;
    }
    /* Phiên đang chạy giữ state; change chỉ cập nhật kết quả probe. */
    if (stored.state != USB_STATE_ACTIVE && stored.state != USB_STATE_CONNECTING) {
        stored.state = ok ? USB_STATE_READY : USB_STATE_FAILED;
    }
    stored.aoap_supported = probed->aoap_supported;
    stored.ncm_present = probed->ncm_present;
    snprintf(stored.net_iface, sizeof(stored.net_iface), "%s", probed->net_iface);
    copy_msg(stored.last_error, sizeof(stored.last_error), detail);
    save_device(manager, &stored, ok ? "phone_ready" : "phone_probe_failed");
    if (announce && ok && stored.state == USB_STATE_READY) {
        usb_session_mode_t preferred = usb_policy_preferred_mode(&stored);
        if (preferred == USB_MODE_ANDROID_AUTO || preferred == USB_MODE_CARPLAY) {
            notify = 1;
            snprintf(device_id, sizeof(device_id), "%s", stored.device_id);
            snprintf(mode, sizeof(mode), "%s", usb_mode_str(preferred));
        }
    }
    if (notify) {
        emit_projection(manager, device_id, mode);
    }
    pthread_mutex_unlock(&manager->mu);
}

static void probe_phone(usb_manager_t* manager, usb_device_t* device, int announce)
{
    usb_session_mode_t mode;
    usb_adapter_t* adapter;
    char msg[USB_ERR_LEN];
    int ok;

    mode = usb_policy_preferred_mode(device);
    pthread_mutex_lock(&manager->mu);
    adapter = adapter_for(manager, mode);
    pthread_mutex_unlock(&manager->mu);
    if (!adapter || !adapter->probe) {
        finish_phone(manager, device, 0, "no_adapter", 0);
        return;
    }
    pthread_mutex_lock(&manager->adapter_mu);
    ok = adapter->probe(device, msg, sizeof(msg), adapter->ctx);
    pthread_mutex_unlock(&manager->adapter_mu);
    finish_phone(manager, device, ok, msg, announce);
}

static void mount_storage(usb_manager_t* manager, usb_device_t* device)
{
    char err[USB_ERR_LEN];
    int ok = usb_storage_attach(device, err, sizeof(err));
    finish_storage(manager, device, ok, ok ? device->mount_point : err);
}

void usb_manager_on_added(usb_manager_t* manager, const usb_device_t* incoming)
{
    usb_device_t device;
    usb_device_t existing;

    if (!manager || !incoming) {
        return;
    }
    device = *incoming;
    if (!device.device_id[0]) {
        usb_assign_id(&device);
    }
    usb_classify_enrich(&device);

    pthread_mutex_lock(&manager->mu);
    if (usb_registry_lookup(manager->registry, &device, &existing)) {
        pthread_mutex_unlock(&manager->mu);
        usb_manager_on_changed(manager, &device);
        return;
    }
    device.state = USB_STATE_ENUMERATING;
    save_device(manager, &device, "add");
    if (device.type == USB_TYPE_UNKNOWN || device.type == USB_TYPE_HID) {
        device.state = USB_STATE_IGNORED;
        save_device(manager, &device, "ignored");
        pthread_mutex_unlock(&manager->mu);
        return;
    }
    device.state = USB_STATE_PROBING;
    save_device(manager, &device, "probing");
    pthread_mutex_unlock(&manager->mu);

    if (device.type == USB_TYPE_MASS_STORAGE) {
        mount_storage(manager, &device);
        return;
    }
    probe_phone(manager, &device, 1);
}

void usb_manager_on_changed(usb_manager_t* manager, const usb_device_t* incoming)
{
    usb_device_t stored;
    usb_device_t fresh;
    int was_ready;
    int had_mount;
    char old_mount[USB_PATH_LEN];

    if (!manager || !incoming) {
        return;
    }
    fresh = *incoming;
    usb_classify_enrich(&fresh);

    pthread_mutex_lock(&manager->mu);
    if (!usb_registry_lookup(manager->registry, &fresh, &stored)) {
        pthread_mutex_unlock(&manager->mu);
        usb_manager_on_added(manager, incoming);
        return;
    }
    /* Giữ id đã phát cho service. Serial có thể chỉ xuất hiện ở event change. */
    snprintf(fresh.device_id, sizeof(fresh.device_id), "%s", stored.device_id);
    fresh.aoap_supported = stored.aoap_supported;
    fresh.ncm_present = stored.ncm_present;
    snprintf(fresh.net_iface, sizeof(fresh.net_iface), "%s", stored.net_iface);
    snprintf(fresh.block_dev, sizeof(fresh.block_dev), "%s", stored.block_dev);
    snprintf(fresh.mount_point, sizeof(fresh.mount_point), "%s", stored.mount_point);
    copy_msg(fresh.last_error, sizeof(fresh.last_error), stored.last_error);
    fresh.state = stored.state;
    was_ready = stored.state == USB_STATE_READY || stored.state == USB_STATE_ACTIVE ||
                stored.state == USB_STATE_CONNECTING;
    had_mount = stored.mount_point[0] != '\0';
    snprintf(old_mount, sizeof(old_mount), "%s", stored.mount_point);
    save_device(manager, &fresh, "change");
    pthread_mutex_unlock(&manager->mu);

    /* Probe hoặc mount của event add vẫn đang chạy. Không mở lần thứ hai. */
    if (fresh.state == USB_STATE_PROBING) {
        return;
    }
    if (fresh.state == USB_STATE_ACTIVE || fresh.state == USB_STATE_CONNECTING) {
        if (fresh.type == USB_TYPE_ANDROID || fresh.type == USB_TYPE_IPHONE) {
            probe_phone(manager, &fresh, 0);
        }
        return;
    }
    if (fresh.type == USB_TYPE_MASS_STORAGE) {
        if (!had_mount) {
            mount_storage(manager, &fresh);
        }
        return;
    }
    if (had_mount) {
        char err[USB_ERR_LEN];
        usb_storage_detach(old_mount, err, sizeof(err));
        fresh.mount_point[0] = '\0';
        fresh.block_dev[0] = '\0';
    }
    if (fresh.type == USB_TYPE_ANDROID || fresh.type == USB_TYPE_IPHONE) {
        probe_phone(manager, &fresh, !was_ready);
    }
}

void usb_manager_on_removed(usb_manager_t* manager, const usb_device_t* incoming)
{
    usb_device_t stored;
    usb_adapter_t* adapter = NULL;
    char mount_point[USB_PATH_LEN];
    int stop_adapter = 0;
    usb_session_t idle;

    if (!manager || !incoming) {
        return;
    }
    pthread_mutex_lock(&manager->mu);
    if (!usb_registry_lookup(manager->registry, incoming, &stored)) {
        pthread_mutex_unlock(&manager->mu);
        return;
    }
    if ((manager->active.state == USB_SESSION_ACTIVE || manager->active.state == USB_SESSION_STARTING) &&
        strcmp(manager->active.device_id, stored.device_id) == 0) {
        adapter = adapter_for(manager, manager->active.mode);
        stop_adapter = 1;
        manager->epoch++;
        clear_session(&manager->active, "unplug");
        idle = manager->active;
    }
    snprintf(mount_point, sizeof(mount_point), "%s", stored.mount_point);
    stored.state = USB_STATE_DISCONNECTING;
    emit_device(manager, &stored, "remove");
    usb_registry_remove(manager->registry, stored.device_id);
    if (stop_adapter) {
        emit_session(manager, &idle);
    }
    pthread_mutex_unlock(&manager->mu);

    if (stop_adapter && adapter && adapter->stop) {
        pthread_mutex_lock(&manager->adapter_mu);
        adapter->stop(&stored, adapter->ctx);
        pthread_mutex_unlock(&manager->adapter_mu);
    }
    if (mount_point[0]) {
        char err[USB_ERR_LEN];
        usb_storage_detach(mount_point, err, sizeof(err));
    }
}

int usb_manager_start_session(usb_manager_t* manager, const char* device_id, usb_session_mode_t mode,
                              char* error_out, size_t error_len)
{
    usb_device_t device;
    usb_policy_decision_t decision;
    usb_adapter_t* adapter;
    usb_session_t starting;
    unsigned epoch_at;
    char msg[USB_ERR_LEN];
    int ok;

    if (!manager || !device_id) {
        copy_msg(error_out, error_len, "invalid_argument");
        return 0;
    }
    pthread_mutex_lock(&manager->mu);
    if (!usb_registry_get(manager->registry, device_id, &device)) {
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, "device_not_found");
        return 0;
    }
    usb_policy_can_start(manager->policy, &device, mode,
                         (manager->active.state == USB_SESSION_IDLE) ? NULL : &manager->active, &decision);
    if (!decision.allowed) {
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, decision.reason);
        USB_LOG_WARN(decision.reason);
        return 0;
    }
    adapter = adapter_for(manager, mode);
    if (!adapter || !adapter->start) {
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, "no_adapter");
        return 0;
    }
    device.state = USB_STATE_CONNECTING;
    save_device(manager, &device, "session_start");
    memset(&starting, 0, sizeof(starting));
    snprintf(starting.device_id, sizeof(starting.device_id), "%s", device_id);
    starting.mode = mode;
    starting.state = USB_SESSION_STARTING;
    copy_msg(starting.reason, sizeof(starting.reason), "starting");
    manager->active = starting;
    epoch_at = manager->epoch;
    emit_session(manager, &starting);
    pthread_mutex_unlock(&manager->mu);

    pthread_mutex_lock(&manager->adapter_mu);
    ok = adapter->start(&device, msg, sizeof(msg), adapter->ctx);
    pthread_mutex_unlock(&manager->adapter_mu);

    pthread_mutex_lock(&manager->mu);
    if (manager->epoch != epoch_at) {
        pthread_mutex_unlock(&manager->mu);
        if (ok && adapter->stop) {
            pthread_mutex_lock(&manager->adapter_mu);
            adapter->stop(&device, adapter->ctx);
            pthread_mutex_unlock(&manager->adapter_mu);
        }
        copy_msg(error_out, error_len, "device_removed");
        return 0;
    }
    if (!still_present(manager, device_id, &device)) {
        clear_session(&manager->active, "device_removed");
        emit_session(manager, &manager->active);
        pthread_mutex_unlock(&manager->mu);
        if (ok && adapter->stop) {
            pthread_mutex_lock(&manager->adapter_mu);
            adapter->stop(&device, adapter->ctx);
            pthread_mutex_unlock(&manager->adapter_mu);
        }
        copy_msg(error_out, error_len, "device_removed");
        return 0;
    }
    if (!ok) {
        device.state = USB_STATE_READY;
        copy_msg(device.last_error, sizeof(device.last_error), msg);
        save_device(manager, &device, "session_failed");
        starting.state = USB_SESSION_FAILED;
        copy_msg(starting.reason, sizeof(starting.reason), msg);
        emit_session(manager, &starting);
        clear_session(&manager->active, "start_failed");
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, msg);
        return 0;
    }
    device.state = USB_STATE_ACTIVE;
    copy_msg(device.last_error, sizeof(device.last_error), msg);
    save_device(manager, &device, "session_active");
    starting.state = USB_SESSION_ACTIVE;
    copy_msg(starting.reason, sizeof(starting.reason), msg);
    manager->active = starting;
    emit_session(manager, &starting);
    pthread_mutex_unlock(&manager->mu);
    return 1;
}

int usb_manager_stop_session(usb_manager_t* manager, const char* device_id, char* error_out,
                             size_t error_len)
{
    usb_device_t device;
    usb_adapter_t* adapter;
    usb_session_mode_t mode;
    usb_session_t stopping;

    if (!manager || !device_id) {
        copy_msg(error_out, error_len, "invalid_argument");
        return 0;
    }
    pthread_mutex_lock(&manager->mu);
    if (!usb_registry_get(manager->registry, device_id, &device)) {
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, "device_not_found");
        return 0;
    }
    if (strcmp(manager->active.device_id, device_id) != 0 ||
        (manager->active.state != USB_SESSION_ACTIVE && manager->active.state != USB_SESSION_STARTING)) {
        pthread_mutex_unlock(&manager->mu);
        copy_msg(error_out, error_len, "session_not_active");
        return 0;
    }
    mode = manager->active.mode;
    adapter = adapter_for(manager, mode);
    device.state = USB_STATE_READY;
    save_device(manager, &device, "session_stopped");
    memset(&stopping, 0, sizeof(stopping));
    snprintf(stopping.device_id, sizeof(stopping.device_id), "%s", device_id);
    stopping.mode = mode;
    stopping.state = USB_SESSION_STOPPING;
    copy_msg(stopping.reason, sizeof(stopping.reason), "stopping");
    emit_session(manager, &stopping);
    manager->epoch++;
    clear_session(&manager->active, "stopped");
    emit_session(manager, &manager->active);
    pthread_mutex_unlock(&manager->mu);

    if (adapter && adapter->stop) {
        pthread_mutex_lock(&manager->adapter_mu);
        adapter->stop(&device, adapter->ctx);
        pthread_mutex_unlock(&manager->adapter_mu);
    }
    return 1;
}

int usb_manager_active(const usb_manager_t* manager, usb_session_t* out)
{
    int ok;

    if (!manager || !out) {
        return 0;
    }
    pthread_mutex_lock((pthread_mutex_t*)&manager->mu);
    ok = manager->active.state == USB_SESSION_ACTIVE || manager->active.state == USB_SESSION_STARTING;
    if (ok) {
        *out = manager->active;
    }
    pthread_mutex_unlock((pthread_mutex_t*)&manager->mu);
    return ok;
}
