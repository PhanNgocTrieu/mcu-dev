SUMMARY = "Allow the lab account to use sudo"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://lab"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/sudoers.d
    install -m 0440 ${WORKDIR}/lab ${D}${sysconfdir}/sudoers.d/lab
}

FILES:${PN} = "${sysconfdir}/sudoers.d/lab"
RDEPENDS:${PN} = "sudo"
