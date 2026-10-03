SUMMARY = "CarPlay media boundary over USB-NCM (IAP2/MFi-ready)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2d3b2102f2a28f3da39ed373d6eee5d2"

inherit cmake externalsrc

EXTERNALSRC = "${TOPDIR}/../../../modules/libhu-carplay"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

# EVB board builds set -DHUPI_WITH_MFI=ON once libiap2/libhu-mfi are imported.
EXTRA_OECMAKE = "-DHUPI_WITH_MFI=OFF"

FILES:${PN}-dev += "${includedir}/hu_carplay.h"
