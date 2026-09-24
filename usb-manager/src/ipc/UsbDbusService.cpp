#include "usb_manager/ipc/UsbDbusService.hpp"
#include "usb_manager/util/Log.hpp"

#include <systemd/sd-bus.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace usb_manager {
namespace {

constexpr const char* kService = "org.example.connectivity";
constexpr const char* kPath = "/org/example/connectivity/usb";
constexpr const char* kIface = "org.example.connectivity.Usb1";

int methodListDevices(sd_bus_message* m, void* userdata, sd_bus_error* retError);
int methodStartSession(sd_bus_message* m, void* userdata, sd_bus_error* retError);
int methodStopSession(sd_bus_message* m, void* userdata, sd_bus_error* retError);
int propGetActiveSession(sd_bus* bus, const char* path, const char* interface,
                         const char* property, sd_bus_message* reply, void* userdata,
                         sd_bus_error* retError);

const sd_bus_vtable kVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("ListDevices", "", "aa{sv}", methodListDevices, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StartSession", "ss", "b", methodStartSession, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StopSession", "s", "b", methodStopSession, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("DeviceChanged", "sss", 0),
    SD_BUS_SIGNAL("SessionChanged", "sss", 0),
    SD_BUS_PROPERTY("ActiveSession", "s", propGetActiveSession, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END};

struct ServiceUserdata {
  DeviceRegistry* registry{nullptr};
  SessionOrchestrator* orchestrator{nullptr};
};

int appendDeviceDict(sd_bus_message* reply, const UsbDeviceInfo& d) {
  int r = sd_bus_message_open_container(reply, 'a', "{sv}");
  if (r < 0) return r;
  auto put = [&](const char* key, const std::string& val) {
    sd_bus_message_open_container(reply, 'e', "sv");
    sd_bus_message_append(reply, "s", key);
    sd_bus_message_open_container(reply, 'v', "s");
    sd_bus_message_append(reply, "s", val.c_str());
    sd_bus_message_close_container(reply);
    sd_bus_message_close_container(reply);
  };
  put("device_id", d.deviceId);
  put("type", toString(d.type));
  put("state", toString(d.state));
  put("manufacturer", d.manufacturer);
  put("product", d.product);
  put("serial", d.serial);
  put("vid", std::to_string(d.vendorId));
  put("pid", std::to_string(d.productId));
  put("net_iface", d.netInterface);
  put("aoap", d.aoapSupported ? "1" : "0");
  put("ncm", d.ncmIfacePresent ? "1" : "0");
  return sd_bus_message_close_container(reply);
}

int methodListDevices(sd_bus_message* m, void* userdata, sd_bus_error* retError) {
  (void)retError;
  auto* ud = static_cast<ServiceUserdata*>(userdata);
  sd_bus_message* reply = nullptr;
  int r = sd_bus_message_new_method_return(m, &reply);
  if (r < 0) return r;
  r = sd_bus_message_open_container(reply, 'a', "a{sv}");
  if (r < 0) return r;
  for (const auto& d : ud->registry->list()) {
    r = appendDeviceDict(reply, d);
    if (r < 0) return r;
  }
  r = sd_bus_message_close_container(reply);
  if (r < 0) return r;
  return sd_bus_send(nullptr, reply, nullptr);
}

int methodStartSession(sd_bus_message* m, void* userdata, sd_bus_error* retError) {
  auto* ud = static_cast<ServiceUserdata*>(userdata);
  const char* deviceId = nullptr;
  const char* modeStr = nullptr;
  int r = sd_bus_message_read(m, "ss", &deviceId, &modeStr);
  if (r < 0) return r;
  auto mode = sessionModeFromString(modeStr ? modeStr : "");
  if (!mode) {
    sd_bus_error_set_const(retError, "org.example.InvalidMode", "unknown mode");
    return -EINVAL;
  }
  std::string err;
  bool ok = ud->orchestrator->startSession(deviceId, *mode, err);
  if (!ok) {
    sd_bus_error_setf(retError, "org.example.StartFailed", "%s", err.c_str());
  }
  return sd_bus_reply_method_return(m, "b", ok ? 1 : 0);
}

int methodStopSession(sd_bus_message* m, void* userdata, sd_bus_error* retError) {
  auto* ud = static_cast<ServiceUserdata*>(userdata);
  const char* deviceId = nullptr;
  int r = sd_bus_message_read(m, "s", &deviceId);
  if (r < 0) return r;
  std::string err;
  bool ok = ud->orchestrator->stopSession(deviceId, err);
  if (!ok) {
    sd_bus_error_setf(retError, "org.example.StopFailed", "%s", err.c_str());
  }
  return sd_bus_reply_method_return(m, "b", ok ? 1 : 0);
}

int propGetActiveSession(sd_bus* bus, const char* path, const char* interface,
                         const char* property, sd_bus_message* reply, void* userdata,
                         sd_bus_error* retError) {
  (void)bus;
  (void)path;
  (void)interface;
  (void)property;
  (void)retError;
  auto* ud = static_cast<ServiceUserdata*>(userdata);
  auto active = ud->orchestrator->activeSession();
  std::string val = "none";
  if (active) {
    val = active->deviceId + ":" + toString(active->mode) + ":" + toString(active->state);
  }
  return sd_bus_message_append(reply, "s", val.c_str());
}

}  // namespace

struct UsbDbusService::Impl {
  DeviceRegistry& registry;
  SessionOrchestrator& orchestrator;
  sd_bus* bus{nullptr};
  sd_bus_slot* slot{nullptr};
  ServiceUserdata ud{};
  std::atomic<bool> running{false};
  std::thread thread;

  Impl(DeviceRegistry& r, SessionOrchestrator& o) : registry(r), orchestrator(o) {
    ud.registry = &registry;
    ud.orchestrator = &orchestrator;
  }

  void loop() {
    while (running.load()) {
      int r = sd_bus_process(bus, nullptr);
      if (r < 0) {
        USB_LOG_ERROR(std::string("sd_bus_process: ") + strerror(-r));
        break;
      }
      if (r > 0) continue;
      r = sd_bus_wait(bus, 200000);  // 200ms
      if (r < 0 && r != -EINTR) {
        USB_LOG_ERROR(std::string("sd_bus_wait: ") + strerror(-r));
        break;
      }
    }
  }
};

UsbDbusService::UsbDbusService(DeviceRegistry& registry, SessionOrchestrator& orchestrator)
    : impl_(std::make_unique<Impl>(registry, orchestrator)) {}

UsbDbusService::~UsbDbusService() { stop(); }

bool UsbDbusService::start() {
  if (impl_->running) return true;
  int r = sd_bus_open_user(&impl_->bus);
  if (r < 0) {
    // Fall back to system bus (typical for embedded)
    r = sd_bus_open_system(&impl_->bus);
  }
  if (r < 0) {
    USB_LOG_WARN(std::string("D-Bus unavailable (") + strerror(-r) +
                 ") — continuing without IPC");
    return false;
  }

  r = sd_bus_add_object_vtable(impl_->bus, &impl_->slot, kPath, kIface, kVtable, &impl_->ud);
  if (r < 0) {
    USB_LOG_ERROR(std::string("sd_bus_add_object_vtable: ") + strerror(-r));
    sd_bus_unref(impl_->bus);
    impl_->bus = nullptr;
    return false;
  }

  r = sd_bus_request_name(impl_->bus, kService, 0);
  if (r < 0) {
    USB_LOG_WARN(std::string("sd_bus_request_name: ") + strerror(-r) +
                 " — object still usable on unique name");
  }

  impl_->running = true;
  impl_->thread = std::thread([this] { impl_->loop(); });
  USB_LOG_INFO(std::string("D-Bus service ready at ") + kPath + " iface " + kIface);
  return true;
}

void UsbDbusService::stop() {
  if (!impl_) return;
  impl_->running = false;
  if (impl_->thread.joinable()) impl_->thread.join();
  if (impl_->slot) {
    sd_bus_slot_unref(impl_->slot);
    impl_->slot = nullptr;
  }
  if (impl_->bus) {
    sd_bus_unref(impl_->bus);
    impl_->bus = nullptr;
  }
}

void UsbDbusService::emitDeviceChanged(const UsbDeviceInfo& device) {
  if (!impl_->bus) return;
  int r = sd_bus_emit_signal(impl_->bus, kPath, kIface, "DeviceChanged", "sss",
                             device.deviceId.c_str(), toString(device.state), toString(device.type));
  if (r < 0) {
    USB_LOG_DEBUG(std::string("emit DeviceChanged failed: ") + strerror(-r));
  }
}

void UsbDbusService::emitSessionChanged(const SessionInfo& session) {
  if (!impl_->bus) return;
  int r = sd_bus_emit_signal(impl_->bus, kPath, kIface, "SessionChanged", "sss",
                             session.deviceId.c_str(), toString(session.state),
                             session.reason.c_str());
  if (r < 0) {
    USB_LOG_DEBUG(std::string("emit SessionChanged failed: ") + strerror(-r));
  }
}

}  // namespace usb_manager
