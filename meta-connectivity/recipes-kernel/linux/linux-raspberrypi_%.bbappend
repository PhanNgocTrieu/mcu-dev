FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://connectivity-usb.cfg"

# Applied when building linux-raspberrypi / linux-yocto with this layer present.
