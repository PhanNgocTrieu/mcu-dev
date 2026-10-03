SUMMARY = "On-screen USB plug/unplug demo panel"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2d3b2102f2a28f3da39ed373d6eee5d2"

inherit cmake externalsrc

DEPENDS = "libsdl2"
RDEPENDS:${PN} = "libsdl2 liberation-fonts usb-man"

EXTERNALSRC = "${TOPDIR}/../../../rasp4/build-demo/apps/usb-panel"
EXTERNALSRC_BUILD = "${WORKDIR}/build"
