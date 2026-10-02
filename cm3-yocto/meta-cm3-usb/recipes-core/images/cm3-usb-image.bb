SUMMARY = "Console image for Compute Module 3 USB host tests"
DESCRIPTION = "Boots to a shell on the Waveshare CM3 PoE board. USB-A ports are host ports on the onboard hub."
LICENSE = "MIT"

inherit core-image

IMAGE_FEATURES += "ssh-server-openssh allow-root-login"

IMAGE_INSTALL:append = " \
    usbutils \
    libusb1 \
    ethtool \
    sudo \
    cm3-sudoers \
    kernel-modules \
    dosfstools \
    e2fsprogs \
    procps \
    util-linux \
    usb-module \
    usb-driver \
"
