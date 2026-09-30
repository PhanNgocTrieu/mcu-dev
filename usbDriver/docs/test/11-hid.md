# 11 — USB HID

IDs: `USBDRV_SWREQ_011001` through `011018`

Do this now on Pi 4 with a USB mouse or keyboard, plugged into USB-A. `CONFIG_USB_HID=y` means the driver may be built-in: `lsmod` not showing `usbhid` is normal.

No mouse and no keyboard: Blocked — missing sample.

## Bind

```bash
echo "=== USBDRV MARK hid ===" | sudo tee /dev/kmsg >/dev/null
# plug the mouse or keyboard
sleep 2
lsusb | tee "$LAB/lsusb-hid.txt"
dmesg -T | awk 'f{print} /USBDRV MARK hid/{f=1}' | tee "$LAB/dmesg-hid.txt"
```

`dmesg` mentions `usbhid` or `input` for the new device.

Interface driver. Take `DEV` from the mouse VID/PID:

```bash
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo none; fi
done
```

The bind part passes when the driver is `usbhid`.

Input list:

```bash
cat /proc/bus/input/devices | tee "$LAB/input-devices.txt"
```

There is a new block for the mouse or keyboard (a `N: Name=` line).

## An event when you touch the device

Find the event node and its name:

```bash
for n in /sys/class/input/event*; do
  echo "$(basename "$n") $(cat "$n/device/name")"
done
```

Pick the `eventX` whose name is the mouse or keyboard you just plugged. Read for a short time while you move the mouse or hold a key:

```bash
sudo hexdump -n 96 -e '16/1 "%02x " "\n"' /dev/input/eventX
```

Hex lines mean input reports arrived. Press Ctrl-C if the command has not stopped after it has printed. That is a Pass.

Silence while nobody touches the device is correct. Do not record Fail for that.

Use `evtest` only if it is already installed. You do not need to install it to Pass: `hexdump` is enough.

## Unplug

Unplug the mouse. That mouse name is gone from `/proc/bus/input/devices`. The `$DEV` path is gone.

## Not enabled

`dmesg` never mentions HID and the sysfs driver is not `usbhid`, for a standard HID mouse (interface class `03` in `lsusb -v`). Enable `CONFIG_USB_HID=y` and rebuild. Do not write a separate mouse driver.
