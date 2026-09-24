SUMMARY = "RPi4 Connectivity SC prototype image with usb-manager"
DESCRIPTION = "Minimal image additions for USB Host projection prototype on raspberrypi4-64."

# bitbake core-image-connectivity
# MACHINE = "raspberrypi4-64" with meta-raspberrypi

require recipes-core/images/core-image-base.bb

IMAGE_FEATURES:append = " ssh-server-openssh"

IMAGE_INSTALL:append = " \
    usb-manager \
    usbutils \
    libusb1 \
"
