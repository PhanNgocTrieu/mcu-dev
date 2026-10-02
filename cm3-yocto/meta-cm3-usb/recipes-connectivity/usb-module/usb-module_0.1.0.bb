SUMMARY = "USB module for enumeration, AOA, and AirPlay transport"
DESCRIPTION = "Userspace session layer above the USB driver library."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ba1233e31cf7e083ee1eb6dc7978c8c"

inherit cmake externalsrc

DEPENDS = "usb-driver"
RDEPENDS:${PN} = "usb-driver"

EXTERNALSRC = "${@os.path.normpath(os.path.join(os.path.dirname(d.getVar('FILE')), '..', '..', '..', '..', 'usbModule'))}"
EXTERNALSRC_BUILD = "${WORKDIR}/usbmod-build"

EXTRA_OECMAKE = "-DUSBMOD_BUILD_TESTS=OFF"
