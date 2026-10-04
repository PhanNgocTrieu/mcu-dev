#!/bin/sh
# Write the Pi 4 SD image. Usage: ./flash-sd.sh /dev/sdX
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /dev/sdX" >&2
    exit 1
fi

disk=$1
root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$root/../.." && pwd)
image=""

# 1) Image đã publish vào repo (nhẹ, dễ share qua git)
if [ -L "$repo/images/pi4/latest.wic.bz2" ] || [ -f "$repo/images/pi4/latest.wic.bz2" ]; then
    image=$(readlink -f "$repo/images/pi4/latest.wic.bz2" 2>/dev/null || echo "$repo/images/pi4/latest.wic.bz2")
fi
# 2) Deploy Yocto local (sau build-image.sh)
if [ -z "$image" ] || [ ! -f "$image" ]; then
    image=$(ls -1t "$root"/build/tmp/deploy/images/raspberrypi4-64/rpi4-usb-image-raspberrypi4-64.rootfs.wic.bz2 2>/dev/null | head -n 1 || true)
fi

if [ -z "$image" ] || [ ! -f "$image" ]; then
    echo "image not found." >&2
    echo "  build:  $root/build-image.sh" >&2
    echo "  publish to repo: $root/publish-image.sh" >&2
    exit 1
fi

case "$disk" in
    /dev/sd[a-z]|/dev/mmcblk[0-9]|/dev/nvme[0-9]n[0-9]) ;;
    *)
        echo "refusing $disk (expected the whole disk)" >&2
        exit 1
        ;;
esac

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
echo "done."
