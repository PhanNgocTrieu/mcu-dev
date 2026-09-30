# 07 — Mass storage

IDs: `USBDRV_SWREQ_007001` through `007018`

Do this now on Pi 4 USB-A with a USB stick. Do not format it and do not write to it.

## Check that the image has the driver

Plug the stick first. If `lsblk` already shows a USB disk, you do not need `modprobe`.

If there is no disk while `lsusb` already sees the stick:

```bash
sudo modprobe usb-storage && echo loaded || echo "NO MODULE"
dmesg | tail -n 30
```

`NO MODULE` means **Not enabled**. On an image you build, add `CONFIG_USB_STORAGE=m`, rebuild, and boot again. On Raspberry Pi OS the module is already there. If `modprobe` fails, keep the full error line and stop this item.

## Steps

Plug the stick into USB-A. If item 01 is already done, plug it again.

```bash
lsblk -o NAME,SIZE,TYPE,TRAN,MODEL,VENDOR | tee "$LAB/lsblk.txt"
```

One disk has `TRAN=usb`. Note the name, for example `sda`. Do not pick the system card (often `mmcblk0`, and it is not `TRAN=usb`).

Driver:

```bash
VID=0781
PID=5581
DEV=$(for d in /sys/bus/usb/devices/[0-9]-[0-9]*; do
  [ -f "$d/idVendor" ] || continue
  [ "$(cat "$d/idVendor")" = "$VID" ] && [ "$(cat "$d/idProduct")" = "$PID" ] && echo "$d" && break
done)
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo none; fi
done
```

Pass when one interface driver is `usb-storage` or `uas`, and `lsblk` shows the stick's disk.

## Read, do not write

Take the partition, for example `sda1`. If the filesystem sits on the whole disk with no partition, use `sda`. Check the `TYPE` column in `lsblk` (`part` or `disk`).

```bash
NODE=/dev/sda1
sudo mkdir -p /mnt/usbdrv-ro
sudo mount -o ro "$NODE" /mnt/usbdrv-ro
find /mnt/usbdrv-ro -type f | head
FILE=$(find /mnt/usbdrv-ro -type f -size -1M | head -n 1)
if [ -n "$FILE" ]; then sha256sum "$FILE" | tee "$LAB/sha256.txt"; fi
sudo umount /mnt/usbdrv-ro
```

The read part passes when `mount` succeeds and `umount` succeeds. An empty filesystem with no file still passes if `mount` works: write "filesystem empty".

If `mount` says the filesystem is unknown, run `sudo blkid "$NODE"` and record the type. Do not run `mkfs`. If the disk has no filesystem you can mount, record Blocked for the read step. The driver plus `lsblk` result still stands on its own.

## Unplug

```bash
# unplug the stick
sleep 2
lsblk -o NAME,TRAN | tee "$LAB/lsblk-after.txt"
test ! -d "$DEV" && echo sysfs-gone
```

Pass when the disk is gone and `$DEV` is gone. The uevent detail is item 12. If you run item 12 next, you do not have to repeat it.

## Do not record Fail for these

- A phone in MTP mode does not create a disk. MTP is not the sample for this item.
- The driver is `uas` rather than `usb-storage`, and the disk can be read: Pass. Record the real driver name.
- `modprobe` is unnecessary because `usb-storage` is built-in (`CONFIG_USB_STORAGE=y`): Pass when the sysfs driver is correct.
