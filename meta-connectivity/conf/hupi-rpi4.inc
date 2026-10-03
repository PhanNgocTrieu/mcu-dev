# Shared Raspberry Pi 4 settings for the connectivity image.
MACHINE ??= "raspberrypi4-64"
INIT_MANAGER = "systemd"
DISTRO_FEATURES:append = " wayland opengl pam"
DISTRO_FEATURES:remove = " x11"
ENABLE_UART = "1"
LICENSE_FLAGS_ACCEPTED += "synaptics-killswitch"
