SUMMARY = "HUPI USB driver userlayer (plug/unplug, enumerate, usbfs lease)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2d3b2102f2a28f3da39ed373d6eee5d2"

inherit cmake systemd externalsrc

EXTERNALSRC = "${TOPDIR}/../../../modules/usb-driver-userlayer"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

EXTRA_OECMAKE = "-DUSBDRV_BUILD_TESTS=OFF"

SYSTEMD_SERVICE:${PN} = "usb-driverd.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/systemd/usb-driverd.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${systemd_system_unitdir}/usb-driverd.service"
