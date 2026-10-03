SUMMARY = "HUPI USB manager (AOA, CarPlay NCM, one session, media boundary)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2d3b2102f2a28f3da39ed373d6eee5d2"

inherit cmake systemd externalsrc

RDEPENDS:${PN} = "usb-driver-userlayer iproute2"

EXTERNALSRC = "${TOPDIR}/../../../modules/usb-man"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

# libhu-aa / libhu-carplay are pulled in through CMake add_subdirectory.
EXTRA_OECMAKE = "-DUSBMAN_BUILD_TESTS=OFF"

SYSTEMD_SERVICE:${PN} = "usb-managerd.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/systemd/usb-managerd.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${systemd_system_unitdir}/usb-managerd.service"
