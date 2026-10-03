#!/bin/sh
# rasp4/build-raspi4 — flashable Raspberry Pi 4 image (same modules as meter-pf).
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$root/../.." && pwd)

mkdir -p "$root/build/conf" "$root/downloads" "$root/sstate-cache"
cp "$root/build-conf/local.conf" "$root/build/conf/local.conf"
cp "$root/build-conf/bblayers.conf" "$root/build/conf/bblayers.conf"

exec docker run --rm --entrypoint bash \
    -v "$repo:/work/mcu-dev" \
    -w /work/mcu-dev/rasp4/build-raspi4 \
    mcu-dev-yocto:scarthgap -lc '
set -eu
set +u
. upstream/poky/oe-init-build-env build >/dev/null
set -u
bitbake rpi4-usb-image
'
