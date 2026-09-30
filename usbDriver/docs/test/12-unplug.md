# 12 — Device removal on unplug

IDs: `USBDRV_SWREQ_012001` through `012018`

Do this now, after item 07 has a disk or item 11 has a mouse. One device is enough to Pass. Repeat with a second device if one is already plugged.

## Unplug after the driver has bound

Use two terminals.

Terminal A:

```bash
sudo udevadm monitor --kernel --property --subsystem-match=usb | tee "$LAB/uevent-unplug.txt"
```

Terminal B, with the stick still plugged. Note the sysfs path and the disk name:

```bash
lsblk -o NAME,TRAN | tee "$LAB/lsblk-before-unplug.txt"
echo "=== USBDRV MARK unplug ===" | sudo tee /dev/kmsg >/dev/null
echo "Unplug the stick now"
```

Unplug the stick. Wait 2 seconds. Terminal B:

```bash
lsusb | tee "$LAB/lsusb-after-unplug.txt"
lsusb -t | tee "$LAB/lsusb-t-after-unplug.txt"
lsblk -o NAME,TRAN | tee "$LAB/lsblk-after-unplug.txt"
```

Check the sysfs path you saved, for example `/sys/bus/usb/devices/2-1`:

```bash
if [ -e /sys/bus/usb/devices/2-1 ]; then echo STILL-THERE; else echo sysfs-gone; fi
```

Replace `2-1` with the real port. If you forgot the port, compare `lsusb -t` before and after: the device branch under that port must be gone.

Pass when all of these are true:

- `lsusb` no longer shows the stick VID/PID (when only one stick of that id was plugged)
- `lsblk` no longer shows the USB disk you just had
- the sysfs path of that port is gone
- `uevent-unplug.txt` contains `ACTION=remove` and `DEVTYPE=usb_device`

Press Ctrl-C on terminal A after the remove line.

For a mouse, use the same criteria and also check that `/proc/bus/input/devices` no longer has that mouse name. For a tty, the matching `/dev/ttyACM*` or `/dev/ttyUSB*` is gone. For a network interface, `ip -br link` no longer shows the interface recorded in item 08 or 09.

## Unplug early

Plug the stick and unplug it within about one second, as soon as the port LED blinks.

Wait another 2 seconds.

```bash
lsusb
lsusb -t
ls /sys/bus/usb/devices
```

Pass when no new device is left hanging on the port you just used. The root hub may remain. A `2-1` (the port you used) must not remain after that port is empty.

If the stick managed to bind and then disappeared, that still passes, as long as no node remains after 2 seconds.

## A second device is not removed

If both a mouse and a stick are plugged: unplug the stick. The mouse remains in `lsusb` and the input device remains. That part passes. If you only have one device, write "did not try two devices". Do not Fail.

## Fail when

- The cable is out, more than 2 seconds have passed, and the disk or the sysfs entry of that port is still there.
- `dmesg` shows an oops or a panic. Keep that `dmesg` and stop the session.
