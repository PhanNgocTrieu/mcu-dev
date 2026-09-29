SUMMARY = "USB host component of connectivity-manager"
DESCRIPTION = "Hotplug, classification, mass-storage mount, and D-Bus for Android Auto and CarPlay services."
HOMEPAGE = "https://example.local/connectivity"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit externalsrc
EXTERNALSRC = "${CONNECTIVITY_USB_SRCDIR}"
EXTERNALSRC_BUILD = "${WORKDIR}/connectivity-usb-build"

DEPENDS = "udev libusb1 systemd sdbus-c++"
RDEPENDS:${PN} += "udev libusb1 usbutils sdbus-c++"

inherit cmake pkgconfig systemd useradd features_check

REQUIRED_DISTRO_FEATURES = "systemd"

USERADD_PACKAGES = "${PN}"
GROUPADD_PARAM:${PN} = "-r plugdev"

SYSTEMD_SERVICE:${PN} = "connectivity-usb.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

FILES:${PN} += "${systemd_system_unitdir}/connectivity-usb.service \
                ${libdir}/udev/rules.d/99-connectivity-usb.rules \
                ${nonarch_base_libdir}/udev/rules.d/99-connectivity-usb.rules"
