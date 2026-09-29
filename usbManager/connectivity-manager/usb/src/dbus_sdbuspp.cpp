#include "dbus_api.h"
#include "log.h"

#include <sdbus-c++/sdbus-c++.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kService = "org.example.connectivity";
constexpr const char* kPath = "/org/example/connectivity/usb";
constexpr const char* kIface = "org.example.connectivity.Usb1";

using Dict = std::map<std::string, sdbus::Variant>;

class UsbService {
 public:
    usb_registry_t* registry{nullptr};
    usb_manager_t* manager{nullptr};
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject> object;

    std::vector<Dict> listDevices() const
    {
        usb_device_t devices[USB_MAX_DEVICES];
        int count = usb_registry_list(registry, devices, USB_MAX_DEVICES);
        std::vector<Dict> out;
        out.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const usb_device_t& device = devices[i];
            char vid[8];
            char pid[8];
            Dict item;
            snprintf(vid, sizeof(vid), "%04x", device.vendor_id);
            snprintf(pid, sizeof(pid), "%04x", device.product_id);
            item["device_id"] = std::string(device.device_id);
            item["type"] = std::string(usb_type_str(device.type));
            item["state"] = std::string(usb_state_str(device.state));
            item["manufacturer"] = std::string(device.manufacturer);
            item["product"] = std::string(device.product);
            item["serial"] = std::string(device.serial);
            item["vid"] = std::string(vid);
            item["pid"] = std::string(pid);
            item["net_iface"] = std::string(device.net_iface);
            item["aoap"] = std::string(device.aoap_supported ? "1" : "0");
            item["ncm"] = std::string(device.ncm_present ? "1" : "0");
            item["block_dev"] = std::string(device.block_dev);
            item["mount_point"] = std::string(device.mount_point);
            item["detail"] = std::string(device.last_error);
            out.push_back(std::move(item));
        }
        return out;
    }

    bool startSession(const std::string& device_id, const std::string& mode_text)
    {
        usb_session_mode_t mode;
        char err[USB_ERR_LEN] = {};

        if (usb_mode_from_str(mode_text.c_str(), &mode) != 0) {
            throw sdbus::Error("org.example.connectivity.Usb1.Error", "unknown mode");
        }
        if (!usb_manager_start_session(manager, device_id.c_str(), mode, err, sizeof(err))) {
            throw sdbus::Error("org.example.connectivity.Usb1.Error", err[0] ? err : "start_failed");
        }
        return true;
    }

    bool stopSession(const std::string& device_id)
    {
        char err[USB_ERR_LEN] = {};

        if (!usb_manager_stop_session(manager, device_id.c_str(), err, sizeof(err))) {
            throw sdbus::Error("org.example.connectivity.Usb1.Error", err[0] ? err : "stop_failed");
        }
        return true;
    }

    std::string activeSession() const
    {
        usb_session_t session;
        if (!usb_manager_active(manager, &session)) {
            return "none";
        }
        return std::string(session.device_id) + ":" + usb_mode_str(session.mode) + ":" +
               usb_session_state_str(session.state);
    }
};

}  // namespace

struct usb_dbus {
    UsbService service;
};

extern "C" usb_dbus_t* usb_dbus_start(usb_registry_t* registry, usb_manager_t* manager)
{
    auto* bus = new usb_dbus();
    bus->service.registry = registry;
    bus->service.manager = manager;
    try {
        try {
            bus->service.conn = sdbus::createSystemBusConnection(kService);
        } catch (const sdbus::Error&) {
            bus->service.conn = sdbus::createSessionBusConnection(kService);
        }
        bus->service.object = sdbus::createObject(*bus->service.conn, kPath);
        UsbService* svc = &bus->service;
        svc->object->registerMethod("ListDevices").onInterface(kIface).implementedAs([svc]() {
            return svc->listDevices();
        });
        svc->object->registerMethod("StartSession")
            .onInterface(kIface)
            .implementedAs([svc](const std::string& id, const std::string& mode) {
                return svc->startSession(id, mode);
            });
        svc->object->registerMethod("StopSession").onInterface(kIface).implementedAs(
            [svc](const std::string& id) { return svc->stopSession(id); });
        svc->object->registerSignal("DeviceChanged")
            .onInterface(kIface)
            .withParameters<std::string, std::string, std::string>();
        svc->object->registerSignal("SessionChanged")
            .onInterface(kIface)
            .withParameters<std::string, std::string, std::string>();
        svc->object->registerSignal("ProjectionAvailable")
            .onInterface(kIface)
            .withParameters<std::string, std::string>();
        svc->object->registerProperty("ActiveSession").onInterface(kIface).withGetter([svc]() {
            return svc->activeSession();
        });
        svc->object->finishRegistration();
        svc->conn->enterEventLoopAsync();
        USB_LOG_INFO("D-Bus (sdbus-c++) ready on org.example.connectivity");
        return bus;
    } catch (const sdbus::Error& err) {
        USB_LOG_WARN(err.what());
        delete bus;
        return nullptr;
    } catch (const std::exception& err) {
        USB_LOG_WARN(err.what());
        delete bus;
        return nullptr;
    }
}

extern "C" void usb_dbus_stop(usb_dbus_t* bus)
{
    if (!bus) {
        return;
    }
    bus->service.object.reset();
    bus->service.conn.reset();
    delete bus;
}

extern "C" void usb_dbus_emit_device(usb_dbus_t* bus, const usb_device_t* device)
{
    if (!bus || !device || !bus->service.object) {
        return;
    }
    try {
        bus->service.object->emitSignal("DeviceChanged")
            .onInterface(kIface)
            .withArguments(std::string(device->device_id), std::string(usb_state_str(device->state)),
                           std::string(usb_type_str(device->type)));
    } catch (const sdbus::Error& err) {
        USB_LOG_WARN(err.what());
    }
}

extern "C" void usb_dbus_emit_session(usb_dbus_t* bus, const usb_session_t* session)
{
    if (!bus || !session || !bus->service.object) {
        return;
    }
    try {
        bus->service.object->emitSignal("SessionChanged")
            .onInterface(kIface)
            .withArguments(std::string(session->device_id), std::string(usb_session_state_str(session->state)),
                           std::string(session->reason));
    } catch (const sdbus::Error& err) {
        USB_LOG_WARN(err.what());
    }
}

extern "C" void usb_dbus_emit_projection(usb_dbus_t* bus, const char* device_id, const char* mode)
{
    if (!bus || !device_id || !mode || !bus->service.object) {
        return;
    }
    try {
        bus->service.object->emitSignal("ProjectionAvailable")
            .onInterface(kIface)
            .withArguments(std::string(device_id), std::string(mode));
    } catch (const sdbus::Error& err) {
        USB_LOG_WARN(err.what());
    }
}
