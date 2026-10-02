SUMMARY = "USB driver boundary library"
DESCRIPTION = "Reads Linux USB sysfs, uevents, and usbfs. Class drivers stay in the kernel."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2ba1233e31cf7e083ee1eb6dc7978c8c"

inherit cmake externalsrc

EXTERNALSRC = "${@os.path.normpath(os.path.join(os.path.dirname(d.getVar('FILE')), '..', '..', '..', '..', 'usbDriver'))}"
EXTERNALSRC_BUILD = "${WORKDIR}/usbdrv-build"

EXTRA_OECMAKE = "-DUSBDRV_BUILD_TESTS=OFF"
