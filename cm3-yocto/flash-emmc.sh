#!/bin/sh
# Write the built image onto the CM3 eMMC.
# The disk must already be the mass-storage device exposed by rpiboot.
# Usage: ./flash-emmc.sh /dev/sdX
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /dev/sdX" >&2
    exit 1
fi

disk=$1
root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
image=$(ls -1t "$root"/build/tmp/deploy/images/raspberrypi-cm3/cm3-usb-image-raspberrypi-cm3.rootfs.wic.bz2 2>/dev/null | head -n 1 || true)

if [ -z "$image" ]; then
    echo "image not found. Build it first with: bitbake cm3-usb-image" >&2
    exit 1
fi

case "$disk" in
    /dev/sd[a-z]|/dev/mmcblk[0-9]) ;;
    *)
        echo "refusing $disk (expected /dev/sdX or /dev/mmcblkN, not a partition)" >&2
        exit 1
        ;;
esac

rootdisk=$(findmnt -n -o SOURCE / | sed 's/[0-9]*$//; s/p[0-9]*$//')
if [ "$disk" = "$rootdisk" ]; then
    echo "refusing to write the host system disk $disk" >&2
    exit 1
fi

echo "image: $image"
echo "disk:  $disk"
printf 'This erases %s. Type the disk name again to continue: ' "$disk"
read -r confirm
if [ "$confirm" != "$disk" ]; then
    echo "aborted" >&2
    exit 1
fi

bzcat "$image" | dd of="$disk" bs=4M conv=fsync status=progress
sync
echo "done. Unplug USB-B, power-cycle the board, and boot from eMMC."
