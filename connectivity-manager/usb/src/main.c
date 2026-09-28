#include "adapter.h"
#include "dbus_api.h"
#include "hotplug.h"
#include "log.h"
#include "manager.h"
#include "policy.h"
#include "registry.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t g_running = 1;

static void on_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static void on_hotplug(usb_hotplug_action_t action, const usb_device_t* device, void* user)
{
    usb_manager_t* manager = user;

    if (action == USB_HOTPLUG_REMOVE) {
        usb_manager_on_removed(manager, device);
    } else if (action == USB_HOTPLUG_CHANGE) {
        usb_manager_on_changed(manager, device);
    } else {
        usb_manager_on_added(manager, device);
    }
}

static void on_device(const usb_device_t* device, const char* reason, void* user)
{
    (void)reason;
    usb_dbus_emit_device(user, device);
}

static void on_session(const usb_session_t* session, void* user)
{
    usb_dbus_emit_session(user, session);
}

static void on_projection(const char* device_id, const char* mode, void* user)
{
    usb_dbus_emit_projection(user, device_id, mode);
}

int main(int argc, char** argv)
{
    int enable_dbus = 1;
    int auto_enum = 1;
    const char* allow_serial = NULL;
    usb_registry_t* registry;
    usb_policy_t* policy;
    usb_manager_t* manager;
    usb_adapter_t android_auto;
    usb_adapter_t carplay;
    usb_manager_listener_t listener;
    usb_dbus_t* bus = NULL;
    usb_udev_monitor_t* monitor;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                    "Usage: %s [--no-dbus] [--no-auto-enum] [--allowlist SERIAL] [--debug]\n"
                    "USB component of connectivity-manager. Android Auto and CarPlay\n"
                    "services call StartSession themselves after ProjectionAvailable.\n",
                    argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--no-dbus") == 0) {
            enable_dbus = 0;
        } else if (strcmp(argv[i], "--no-auto-enum") == 0) {
            auto_enum = 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            usb_log_set_level(USB_LOG_DEBUG);
        } else if (strcmp(argv[i], "--allowlist") == 0 && i + 1 < argc) {
            allow_serial = argv[++i];
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    registry = usb_registry_create();
    policy = usb_policy_create();
    if (allow_serial) {
        usb_policy_set_allowlist(policy, 1);
        usb_policy_allow_serial(policy, allow_serial);
    }
    manager = usb_manager_create(registry, policy);
    usb_adapter_android_init(&android_auto);
    usb_adapter_carplay_init(&carplay);
    usb_manager_set_adapters(manager, &android_auto, &carplay);

    if (enable_dbus) {
        bus = usb_dbus_start(registry, manager);
    }
    memset(&listener, 0, sizeof(listener));
    listener.on_device = on_device;
    listener.on_session = on_session;
    listener.on_projection = on_projection;
    listener.user = bus;
    usb_manager_set_listener(manager, &listener);

    monitor = usb_udev_monitor_create();
    usb_udev_monitor_set_callback(monitor, on_hotplug, manager);
    if (!usb_udev_monitor_start(monitor)) {
        USB_LOG_ERROR("Failed to start USB hotplug monitor");
        usb_dbus_stop(bus);
        usb_udev_monitor_destroy(monitor);
        usb_adapter_shutdown(&android_auto);
        usb_adapter_shutdown(&carplay);
        usb_manager_destroy(manager);
        usb_policy_destroy(policy);
        usb_registry_destroy(registry);
        return 1;
    }
    if (auto_enum) {
        usb_udev_monitor_enumerate(monitor);
    }

    USB_LOG_INFO("connectivity-usb running (USB Host inside connectivity manager)");
    while (g_running) {
        usb_udev_monitor_poll(monitor, 500);
    }

    USB_LOG_INFO("Shutting down");
    usb_dbus_stop(bus);
    usb_udev_monitor_destroy(monitor);
    usb_adapter_shutdown(&android_auto);
    usb_adapter_shutdown(&carplay);
    usb_manager_destroy(manager);
    usb_policy_destroy(policy);
    usb_registry_destroy(registry);
    return 0;
}
