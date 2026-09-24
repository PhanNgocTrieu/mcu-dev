#include "usb_manager/hotplug/UdevMonitor.hpp"
#include "usb_manager/classify/DeviceClassifier.hpp"
#include "usb_manager/util/Log.hpp"

#include <libudev.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>

namespace usb_manager {
namespace {

uint16_t parseHexId(const char* s) {
  if (!s || !*s) return 0;
  unsigned v = 0;
  std::sscanf(s, "%x", &v);
  return static_cast<uint16_t>(v);
}

std::string makeDeviceId(udev_device* dev) {
  const char* syspath = udev_device_get_syspath(dev);
  const char* bus = udev_device_get_sysattr_value(dev, "busnum");
  const char* addr = udev_device_get_sysattr_value(dev, "devnum");
  if (bus && addr) {
    std::ostringstream oss;
    oss << "usb-" << bus << "-" << addr;
    return oss.str();
  }
  if (syspath) return std::string("sys:") + syspath;
  return "usb-unknown";
}

UsbDeviceInfo fillFromUdev(udev_device* dev) {
  UsbDeviceInfo info;
  info.sysPath = udev_device_get_syspath(dev) ? udev_device_get_syspath(dev) : "";
  info.devNode = udev_device_get_devnode(dev) ? udev_device_get_devnode(dev) : "";
  info.deviceId = makeDeviceId(dev);

  const char* vid = udev_device_get_property_value(dev, "ID_VENDOR_ID");
  const char* pid = udev_device_get_property_value(dev, "ID_MODEL_ID");
  if (!vid) vid = udev_device_get_sysattr_value(dev, "idVendor");
  if (!pid) pid = udev_device_get_sysattr_value(dev, "idProduct");
  info.vendorId = parseHexId(vid);
  info.productId = parseHexId(pid);

  const char* mfr = udev_device_get_property_value(dev, "ID_VENDOR");
  if (!mfr) mfr = udev_device_get_sysattr_value(dev, "manufacturer");
  const char* prod = udev_device_get_property_value(dev, "ID_MODEL");
  if (!prod) prod = udev_device_get_sysattr_value(dev, "product");
  const char* serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
  if (!serial) serial = udev_device_get_sysattr_value(dev, "serial");
  const char* ifaces = udev_device_get_property_value(dev, "ID_USB_INTERFACES");

  info.manufacturer = mfr ? mfr : "";
  info.product = prod ? prod : "";
  info.serial = serial ? serial : "";
  info.interfaces = ifaces ? ifaces : "";
  info.state = DeviceState::Enumerating;
  return info;
}

bool isUsbDevice(udev_device* dev) {
  const char* subsystem = udev_device_get_subsystem(dev);
  const char* devtype = udev_device_get_devtype(dev);
  return subsystem && std::strcmp(subsystem, "usb") == 0 &&
         devtype && std::strcmp(devtype, "usb_device") == 0;
}

}  // namespace

struct UdevMonitor::Impl {
  udev* udevCtx{nullptr};
  udev_monitor* mon{nullptr};
  HotplugCallback cb;
  DeviceClassifier classifier;
  bool running{false};
};

UdevMonitor::UdevMonitor() : impl_(std::make_unique<Impl>()) {}

UdevMonitor::~UdevMonitor() { stop(); }

void UdevMonitor::setCallback(HotplugCallback cb) { impl_->cb = std::move(cb); }

bool UdevMonitor::start() {
  if (impl_->running) return true;
  impl_->udevCtx = udev_new();
  if (!impl_->udevCtx) {
    USB_LOG_ERROR("udev_new failed");
    return false;
  }
  impl_->mon = udev_monitor_new_from_netlink(impl_->udevCtx, "udev");
  if (!impl_->mon) {
    USB_LOG_ERROR("udev_monitor_new_from_netlink failed");
    udev_unref(impl_->udevCtx);
    impl_->udevCtx = nullptr;
    return false;
  }
  udev_monitor_filter_add_match_subsystem_devtype(impl_->mon, "usb", "usb_device");
  udev_monitor_enable_receiving(impl_->mon);
  impl_->running = true;
  USB_LOG_INFO("UdevMonitor started");
  return true;
}

void UdevMonitor::stop() {
  if (!impl_) return;
  if (impl_->mon) {
    udev_monitor_unref(impl_->mon);
    impl_->mon = nullptr;
  }
  if (impl_->udevCtx) {
    udev_unref(impl_->udevCtx);
    impl_->udevCtx = nullptr;
  }
  impl_->running = false;
}

int UdevMonitor::fd() const {
  if (!impl_->mon) return -1;
  return udev_monitor_get_fd(impl_->mon);
}

bool UdevMonitor::pollOnce(int timeoutMs) {
  if (!impl_->mon || !impl_->cb) return false;

  int fd = udev_monitor_get_fd(impl_->mon);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  timeval* ptv = timeoutMs < 0 ? nullptr : &tv;
  int ret = select(fd + 1, &fds, nullptr, nullptr, ptv);
  if (ret <= 0 || !FD_ISSET(fd, &fds)) return false;

  udev_device* dev = udev_monitor_receive_device(impl_->mon);
  if (!dev) return false;
  if (!isUsbDevice(dev)) {
    udev_device_unref(dev);
    return false;
  }

  const char* action = udev_device_get_action(dev);
  HotplugEvent ev;
  if (action && std::strcmp(action, "remove") == 0) {
    ev.action = HotplugAction::Remove;
  } else if (action && std::strcmp(action, "change") == 0) {
    ev.action = HotplugAction::Change;
  } else {
    ev.action = HotplugAction::Add;
  }

  ev.device = fillFromUdev(dev);
  impl_->classifier.enrich(ev.device);
  USB_LOG_INFO(std::string("hotplug ") +
               (ev.action == HotplugAction::Remove ? "remove" : "add/change") + " " +
               ev.device.deviceId + " " + toString(ev.device.type) +
               " vid=" + std::to_string(ev.device.vendorId) +
               " pid=" + std::to_string(ev.device.productId));
  impl_->cb(ev);
  udev_device_unref(dev);
  return true;
}

void UdevMonitor::enumerateExisting() {
  if (!impl_->udevCtx || !impl_->cb) return;
  udev_enumerate* enumerate = udev_enumerate_new(impl_->udevCtx);
  if (!enumerate) return;
  udev_enumerate_add_match_subsystem(enumerate, "usb");
  udev_enumerate_scan_devices(enumerate);
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry* entry = nullptr;
  udev_list_entry_foreach(entry, devices) {
    const char* path = udev_list_entry_get_name(entry);
    udev_device* dev = udev_device_new_from_syspath(impl_->udevCtx, path);
    if (!dev) continue;
    if (isUsbDevice(dev)) {
      HotplugEvent ev;
      ev.action = HotplugAction::Add;
      ev.device = fillFromUdev(dev);
      impl_->classifier.enrich(ev.device);
      USB_LOG_DEBUG("enumerate " + ev.device.deviceId + " type=" + toString(ev.device.type));
      impl_->cb(ev);
    }
    udev_device_unref(dev);
  }
  udev_enumerate_unref(enumerate);
}

}  // namespace usb_manager
