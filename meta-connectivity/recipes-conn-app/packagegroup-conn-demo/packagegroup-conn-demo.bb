SUMMARY = "Connectivity demo package group"
LICENSE = "MIT"

inherit packagegroup

RDEPENDS:${PN} = " \
    usb-driver-userlayer \
    usb-man \
    libhu-aa \
    libhu-carplay \
    hupi-cluster \
    usb-demo \
"
