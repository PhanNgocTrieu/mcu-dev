SUMMARY = "USB Manager daemon for Connectivity SC (Host)"
DESCRIPTION = "USB hotplug, classification, Android Auto demo session, CarPlay stub."
HOMEPAGE = "https://example.local/connectivity"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Build sources from the monorepo tree next to this layer:
#   mcu-dev/usb-manager  +  mcu-dev/meta-connectivity
inherit externalsrc
EXTERNALSRC = "${USB_MANAGER_SRCDIR}"
EXTERNALSRC_BUILD = "${WORKDIR}/usb-manager-build"

DEPENDS = "udev libusb1 systemd"
RDEPENDS:${PN} += "udev libusb1 usbutils"

inherit cmake pkgconfig systemd

SYSTEMD_SERVICE:${PN} = "usb-manager.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

# CMake already installs the unit and udev rules; ensure package claims them.
FILES:${PN} += "${systemd_system_unitdir}/usb-manager.service \
                ${libdir}/udev/rules.d/99-connectivity-usb.rules \
                ${nonarch_base_libdir}/udev/rules.d/99-connectivity-usb.rules"
