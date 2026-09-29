SUMMARY = "RPi4 Connectivity SC prototype image with connectivity-usb"
DESCRIPTION = "Minimal image additions for USB Host projection prototype on raspberrypi4-64."

# bitbake core-image-connectivity
# MACHINE = "raspberrypi4-64" with meta-raspberrypi

require recipes-core/images/core-image-base.bb

# Prototype board login: ssh root@<ip> with an empty password.
# debug-tweaks still enables empty-root-password, allow-empty-password,
# and allow-root-login on scarthgap.
IMAGE_FEATURES:append = " ssh-server-openssh debug-tweaks"

IMAGE_INSTALL:append = " \
    connectivity-usb \
    usbutils \
    libusb1 \
    dbus \
    kernel-modules \
"
