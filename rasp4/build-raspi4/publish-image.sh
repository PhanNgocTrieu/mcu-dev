#!/bin/sh
# Copy flashable Pi 4 image into repo images/pi4/ for versioning / flash without Yocto path.
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$root/../.." && pwd)
deploy="$root/build/tmp/deploy/images/raspberrypi4-64"
dest="$repo/images/pi4"

if [ ! -d "$deploy" ]; then
    echo "deploy dir missing. Run: $root/build-image.sh" >&2
    exit 1
fi

wic=$(ls -1t "$deploy"/rpi4-usb-image-raspberrypi4-64.rootfs*.wic.bz2 2>/dev/null | grep -v '\.wic\.bmap' | head -n 1 || true)
if [ -z "$wic" ]; then
    wic=$(ls -1t "$deploy"/rpi4-usb-image-raspberrypi4-64.rootfs.wic.bz2 2>/dev/null | head -n 1 || true)
fi
if [ -z "$wic" ] || [ ! -e "$wic" ]; then
    echo "no .wic.bz2 under $deploy" >&2
    exit 1
fi
# Deploy thường dùng symlink → lấy file thật (bytes) để commit git / flash độc lập build tree
wic_real=$(readlink -f "$wic" || echo "$wic")
if [ ! -f "$wic_real" ]; then
    echo "cannot resolve image: $wic" >&2
    exit 1
fi

mkdir -p "$dest"
base=$(basename "$wic_real")
target="$dest/$base"
stable="$dest/rpi4-usb-image-raspberrypi4-64.rootfs.wic.bz2"

if [ -f "$target" ] && cmp -s "$wic_real" "$target"; then
    echo "unchanged: $target"
else
    cp -f "$wic_real" "$target"
    echo "copied: $target"
fi
ln -sf "$base" "$stable"

bmap="${wic_real%.bz2}.bmap"
if [ -f "$bmap" ]; then
    bmap_base=$(basename "$bmap")
    bmap_target="$dest/$bmap_base"
    if [ ! -f "$bmap_target" ] || ! cmp -s "$bmap" "$bmap_target"; then
        cp -f "$bmap" "$bmap_target"
        echo "copied: $bmap_target"
    fi
fi

ln -sf "$base" "$dest/latest.wic.bz2"
if [ -n "${bmap_base:-}" ] && [ -f "$dest/$bmap_base" ]; then
    ln -sf "$bmap_base" "$dest/latest.wic.bmap"
fi

size=$(du -h "$target" | awk '{print $1}')
echo "publish ok ($size) → images/pi4/$base"
echo "flash: $root/flash-sd.sh /dev/sdX"
