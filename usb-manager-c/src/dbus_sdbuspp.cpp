#include "dbus_api.h"
#include "log.h"

#include <sdbus-c++/sdbus-c++.h>

#include <cstdint>
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
  usb_orchestrator_t* orch{nullptr};
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<sdbus::IObject> object;

  std::vector<Dict> listDevices() const {
    usb_device_t devices[USB_MAX_DEVICES];
    int n = usb_registry_list(registry, devices, USB_MAX_DEVICES);
    std::vector<Dict> out;
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      const usb_device_t& d = devices[i];
      Dict item;
      item["device_id"] = std::string(d.device_id);
      item["type"] = std::string(usb_type_str(d.type));
      item["state"] = std::string(usb_state_str(d.state));
      item["manufacturer"] = std::string(d.manufacturer);
      item["product"] = std::string(d.product);
      item["serial"] = std::string(d.serial);
      item["vid"] = std::to_string(d.vendor_id);
      item["pid"] = std::to_string(d.product_id);
      item["net_iface"] = std::string(d.net_iface);
      item["aoap"] = std::string(d.aoap_supported ? "1" : "0");
      item["ncm"] = std::string(d.ncm_present ? "1" : "0");
      out.push_back(std::move(item));
    }
    return out;
  }

  bool startSession(const std::string& deviceId, const std::string& modeStr) {
    usb_session_mode_t mode;
    if (usb_mode_from_str(modeStr.c_str(), &mode) != 0) {
      throw sdbus::Error("org.example.InvalidMode", "unknown mode");
    }
    char err[USB_ERR_LEN] = {};
    return usb_orchestrator_start(orch, deviceId.c_str(), mode, err, sizeof(err)) != 0;
  }

  bool stopSession(const std::string& deviceId) {
    char err[USB_ERR_LEN] = {};
    return usb_orchestrator_stop(orch, deviceId.c_str(), err, sizeof(err)) != 0;
  }

  std::string activeSession() const {
    usb_session_t session;
    if (!usb_orchestrator_active(orch, &session)) return "none";
    return std::string(session.device_id) + ":" + usb_mode_str(session.mode) + ":" +
           usb_session_state_str(session.state);
  }
};

}  // namespace

struct usb_dbus {
  UsbService service;
};

extern "C" usb_dbus_t* usb_dbus_start(usb_registry_t* registry, usb_orchestrator_t* orch) {
  auto* bus = new usb_dbus();
  bus->service.registry = registry;
  bus->service.orch = orch;
  try {
    try {
      bus->service.conn = sdbus::createSessionBusConnection(kService);
    } catch (const sdbus::Error&) {
      bus->service.conn = sdbus::createSystemBusConnection(kService);
    }
    bus->service.object = sdbus::createObject(*bus->service.conn, kPath);
    UsbService* svc = &bus->service;
    svc->object->registerMethod("ListDevices").onInterface(kIface).implementedAs([svc]() {
      return svc->listDevices();
    });
    svc->object->registerMethod("StartSession").onInterface(kIface).implementedAs(
        [svc](const std::string& id, const std::string& mode) { return svc->startSession(id, mode); });
    svc->object->registerMethod("StopSession").onInterface(kIface).implementedAs(
        [svc](const std::string& id) { return svc->stopSession(id); });
    svc->object->registerSignal("DeviceChanged")
        .onInterface(kIface)
        .withParameters<std::string, std::string, std::string>();
    svc->object->registerSignal("SessionChanged")
        .onInterface(kIface)
        .withParameters<std::string, std::string, std::string>();
    svc->object->registerProperty("ActiveSession").onInterface(kIface).withGetter([svc]() {
      return svc->activeSession();
    });
    svc->object->finishRegistration();
    svc->conn->enterEventLoopAsync();
    USB_LOG_INFO("D-Bus (sdbus-c++) ready");
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

extern "C" void usb_dbus_stop(usb_dbus_t* bus) {
  if (!bus) return;
  bus->service.object.reset();
  bus->service.conn.reset();
  delete bus;
}

extern "C" void usb_dbus_emit_device(usb_dbus_t* bus, const usb_device_t* device) {
  if (!bus || !device || !bus->service.object) return;
  try {
    bus->service.object->emitSignal("DeviceChanged")
        .onInterface(kIface)
        .withArguments(std::string(device->device_id), std::string(usb_state_str(device->state)),
                       std::string(usb_type_str(device->type)));
  } catch (const sdbus::Error& err) {
    USB_LOG_WARN(err.what());
  }
}

extern "C" void usb_dbus_emit_session(usb_dbus_t* bus, const usb_session_t* session) {
  if (!bus || !session || !bus->service.object) return;
  try {
    bus->service.object->emitSignal("SessionChanged")
        .onInterface(kIface)
        .withArguments(std::string(session->device_id), std::string(usb_session_state_str(session->state)),
                       std::string(session->reason));
  } catch (const sdbus::Error& err) {
    USB_LOG_WARN(err.what());
  }
}
