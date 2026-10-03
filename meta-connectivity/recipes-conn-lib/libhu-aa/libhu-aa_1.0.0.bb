SUMMARY = "Android Auto media boundary (AASDK-ready)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2d3b2102f2a28f3da39ed373d6eee5d2"

inherit cmake externalsrc

EXTERNALSRC = "${TOPDIR}/../../../modules/libhu-aa"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

# Flip to ON and provide third_party/aasdk on the EVB/board image when the
# proprietary or f1xpl AASDK tree is available in the build.
EXTRA_OECMAKE = "-DHUPI_WITH_AASDK=OFF"

FILES:${PN}-dev += "${includedir}/hu_aa.h"
