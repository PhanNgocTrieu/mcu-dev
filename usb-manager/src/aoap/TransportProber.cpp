#include "usb_manager/aoap/TransportProber.hpp"
#include "usb_manager/util/Log.hpp"

#include <libusb-1.0/libusb.h>

#include <climits>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace usb_manager {
namespace {

// AOAP protocol — Android Open Accessory
// GET_PROTOCOL = 51, SEND_STRING = 52, START = 53
constexpr uint8_t kAoapGetProtocol = 51;
constexpr uint16_t kGoogleVid = 0x18d1;
constexpr uint16_t kAoapPidLow = 0x2d00;
constexpr uint16_t kAoapPidHigh = 0x2d05;

bool alreadyInAoapMode(uint16_t vid, uint16_t pid) {
  return vid == kGoogleVid && pid >= kAoapPidLow && pid <= kAoapPidHigh;
}

std::string readSysfsFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) return {};
  std::string s;
  std::getline(in, s);
  return s;
}

}  // namespace

AoapProbeResult TransportProber::probeAoap(const UsbDeviceInfo& device) const {
  AoapProbeResult result;
  result.attempted = true;

  if (alreadyInAoapMode(device.vendorId, device.productId)) {
    result.supported = true;
    result.detail = "already_in_aoap_mode";
    USB_LOG_INFO("AOAP: device " + device.deviceId + " already in accessory mode");
    return result;
  }

  libusb_context* ctx = nullptr;
  if (libusb_init(&ctx) != 0) {
    result.detail = "libusb_init_failed";
    USB_LOG_WARN("AOAP probe: libusb_init failed");
    return result;
  }

  libusb_device_handle* handle =
      libusb_open_device_with_vid_pid(ctx, device.vendorId, device.productId);
  if (!handle) {
    result.detail = "open_failed_need_permissions_or_device_busy";
    USB_LOG_WARN("AOAP probe: cannot open " + device.deviceId +
                 " (permissions/busy). Logging only.");
    libusb_exit(ctx);
    return result;
  }

  // Vendor IN request: GET_PROTOCOL
  uint16_t protocol = 0;
  int rc = libusb_control_transfer(
      handle,
      /*bmRequestType*/ 0xC0,  // device-to-host | vendor | device
      kAoapGetProtocol,
      0,
      0,
      reinterpret_cast<unsigned char*>(&protocol),
      sizeof(protocol),
      1000);

  if (rc == sizeof(protocol) && protocol >= 1) {
    result.supported = true;
    std::ostringstream oss;
    oss << "aoap_protocol=" << protocol;
    result.detail = oss.str();
    USB_LOG_INFO("AOAP: " + device.deviceId + " supports protocol " + std::to_string(protocol));
  } else {
    result.supported = false;
    result.detail = "get_protocol_failed_rc=" + std::to_string(rc);
    USB_LOG_INFO("AOAP: " + device.deviceId + " GET_PROTOCOL failed rc=" + std::to_string(rc));
  }

  libusb_close(handle);
  libusb_exit(ctx);
  return result;
}

NcmProbeResult TransportProber::probeNcm(const UsbDeviceInfo& device) const {
  NcmProbeResult result;
  // Scan /sys/class/net for usb* / eth* interfaces whose device links into our USB sysfs tree
  DIR* dir = opendir("/sys/class/net");
  if (!dir) {
    result.detail = "no_sys_class_net";
    return result;
  }

  const std::string& sysPath = device.sysPath;
  while (dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.') continue;
    std::string ifName = ent->d_name;
    std::string linkPath = std::string("/sys/class/net/") + ifName + "/device";
    char buf[PATH_MAX];
    ssize_t n = readlink(linkPath.c_str(), buf, sizeof(buf) - 1);
    if (n < 0) continue;
    buf[n] = '\0';
    std::string resolved = buf;
    // Also check uevent DEVPATH style — if interface device path contains usb bus path
    std::string abs;
    if (resolved[0] != '/') {
      abs = std::string("/sys/class/net/") + ifName + "/" + resolved;
    } else {
      abs = resolved;
    }
    char real[PATH_MAX];
    if (realpath(abs.c_str(), real)) {
      abs = real;
    }
    bool match = false;
    if (!sysPath.empty() && abs.find(sysPath) != std::string::npos) {
      match = true;
    }
    // Heuristic: usb0/usb1 or rndis interfaces often appear with phones
    std::string driver = readSysfsFile(std::string("/sys/class/net/") + ifName + "/device/driver/uevent");
    std::string ifaceDriver = readSysfsFile(std::string("/sys/class/net/") + ifName + "/uevent");
    if (!match) {
      // Fallback: look for cdc_ncm / rndis_host in device tree near this interface
      std::ifstream drv(std::string("/sys/class/net/") + ifName + "/device/driver");
      // readlink on driver
      char dbuf[PATH_MAX];
      std::string driverLink = std::string("/sys/class/net/") + ifName + "/device/driver";
      ssize_t dn = readlink(driverLink.c_str(), dbuf, sizeof(dbuf) - 1);
      if (dn > 0) {
        dbuf[dn] = '\0';
        std::string dname = dbuf;
        if (dname.find("cdc_ncm") != std::string::npos ||
            dname.find("rndis_host") != std::string::npos ||
            dname.find("cdc_ether") != std::string::npos) {
          // Without sysPath match we only accept if vendor is phone-like — still log
          if (device.type == DeviceType::Android || device.type == DeviceType::IPhone) {
            match = true;
            result.detail = "matched_driver_" + dname;
          }
        }
      }
    }
    if (match) {
      result.ifacePresent = true;
      result.ifName = ifName;
      if (result.detail.empty()) result.detail = "matched_syspath";
      USB_LOG_INFO("NCM/RNDIS: " + device.deviceId + " iface=" + ifName);
      break;
    }
    (void)driver;
    (void)ifaceDriver;
  }
  closedir(dir);

  if (!result.ifacePresent) {
    result.detail = result.detail.empty() ? "no_ncm_rndis_iface" : result.detail;
    USB_LOG_INFO("NCM probe: " + device.deviceId + " " + result.detail);
  }
  return result;
}

}  // namespace usb_manager
