FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://hupi-usb.cfg"
KERNEL_MODULE_AUTOLOAD += "cdc_ncm tun"
