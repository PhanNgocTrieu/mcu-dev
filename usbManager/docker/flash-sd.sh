#!/bin/bash
# Write the connectivity image to a block device.
# Refuses mounted disks and the disk that backs this container's root filesystem.
set -euo pipefail

IMAGE_LINK="/work/yocto/build/tmp/deploy/images/raspberrypi4-64/core-image-connectivity-raspberrypi4-64.rootfs.wic.bz2"
BMAP_LINK="/work/yocto/build/tmp/deploy/images/raspberrypi4-64/core-image-connectivity-raspberrypi4-64.rootfs.wic.bmap"

usage() {
    echo "usage: flash-sd /dev/sdX YES" >&2
    echo "  YES is required so a device name alone cannot wipe a disk." >&2
    echo "  The image is ${IMAGE_LINK}" >&2
}

if [[ $# -ne 2 || "$2" != "YES" ]]; then
    usage
    exit 2
fi

dev="$1"
if [[ ! "${dev}" =~ ^/dev/(sd[a-z]|mmcblk[0-9]+|nvme[0-9]+n[0-9]+)$ ]]; then
    echo "error: ${dev} is not a whole-disk path like /dev/sdX or /dev/mmcblk0" >&2
    exit 2
fi
if [[ ! -b "${dev}" ]]; then
    echo "error: ${dev} is not a block device. Pass it into the container with --privileged and a /dev mount." >&2
    exit 2
fi
if [[ ! -f "${IMAGE_LINK}" ]]; then
    echo "error: image not built yet: ${IMAGE_LINK}" >&2
    echo "Build it with: bitbake core-image-connectivity" >&2
    exit 1
fi

root_src=$(findmnt -n -o SOURCE / || true)
root_disk=""
if [[ -n "${root_src}" ]]; then
    root_disk=$(lsblk -no PKNAME "${root_src}" 2>/dev/null | head -n1 || true)
    if [[ -z "${root_disk}" && "${root_src}" =~ ^/dev/ ]]; then
        root_disk=$(basename "${root_src}")
    fi
fi
if [[ -n "${root_disk}" && "${dev}" == "/dev/${root_disk}" ]]; then
    echo "error: ${dev} holds the root filesystem. Refusing to flash it." >&2
    exit 1
fi

if lsblk -nr -o MOUNTPOINT "${dev}" | grep -q '[^[:space:]]'; then
    echo "error: ${dev} has a mounted filesystem. Unmount it first." >&2
    lsblk "${dev}" >&2
    exit 1
fi

echo "Device:"
lsblk -o NAME,SIZE,TYPE,MODEL,TRAN,MOUNTPOINT "${dev}"
echo "Image: ${IMAGE_LINK}"
if [[ -f "${BMAP_LINK}" ]]; then
    sudo bmaptool copy --bmap "${BMAP_LINK}" "${IMAGE_LINK}" "${dev}"
else
    echo "warning: no .bmap file; copying the compressed image without a map." >&2
    sudo bmaptool copy --nobmap "${IMAGE_LINK}" "${dev}"
fi
sync
echo "done. Insert the card in the Raspberry Pi 4 and power it from a 5V/3A supply."
