SUMMARY = "Raspberry Pi 4 image: separated usb-driver-userlayer + usb-man + demos"
LICENSE = "MIT"

inherit core-image

IMAGE_FEATURES += "weston ssh-server-openssh empty-root-password allow-root-login serial-autologin-root"

IMAGE_INSTALL:append = " \
    usb-driver-userlayer \
    usb-man \
    hupi-cluster \
    usb-demo \
    kernel-modules \
    usbutils \
    libusb1 \
    ethtool \
    iproute2 \
    procps \
    liberation-fonts \
"

IMAGE_ROOTFS_EXTRA_SPACE = "262144"
