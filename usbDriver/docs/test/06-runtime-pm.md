# 06 — Runtime PM

IDs: `USBDRV_SWREQ_006001` through `006018`

Do this now: read `power/control` of the plugged USB stick or phone. Do not require the Pi to enter suspend. The product question "does it use suspend-to-RAM?" is still open. Until that is closed, record the sleep/wake part as Blocked. Do not record Pass and do not record Fail for that part.

## Autosuspend

Plug the stick. Take the sysfs path `DEV` as in the README (`/sys/bus/usb/devices/2-1`).

```bash
DEV=/sys/bus/usb/devices/2-1
echo -n "control="; cat "$DEV/power/control"
echo -n "autosuspend="; cat "$DEV/power/autosuspend" 2>/dev/null || cat "$DEV/power/autosuspend_delay_ms"
echo -n "runtime_status="; cat "$DEV/power/runtime_status"
```

Pass when `control` is `on`.

If `control` is `auto`:

```bash
echo on | sudo tee "$DEV/power/control"
cat "$DEV/power/control"
```

It must read `on`. This is the session check. The way to keep it after every plug, once the policy is fixed, is a udev rule or the kernel command line. Do not patch `usb-storage`:

```bash
# lab rule, file /etc/udev/rules.d/99-usbdrv-pm.rules
# SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{power/control}="on"
```

Or add `usbcore.autosuspend=-1` to the kernel command line and reboot. On a Pi the command line is usually one line in `/boot/firmware/cmdline.txt`. Copy a backup before editing. After reboot, plug the stick. `control` must be `on` without a manual `echo`.

Write down which method you used (sysfs by hand, udev, or the command line).

A bad value is rejected and the old value remains:

```bash
echo banana | sudo tee "$DEV/power/control"; echo exit:$?
cat "$DEV/power/control"
```

`control` is still `on` or `auto`, whichever it was before the bad write.

## System suspend — only if this lab machine may be powered down

Skip this whole section if the Pi is in use for other work, or if the product has not decided that sleep exists. Record Blocked.

If you do run it:

1. Plug the stick. Confirm `lsusb` sees it and `control` is `on`.
2. Start `udevadm monitor` as in the README.
3. `sudo systemctl suspend`
4. Wake it with a keyboard or the Pi power button, depending on whether the image supports wake. If the machine does not wake, remove power the way that image expects, boot again, and write "suspend did not wake on this Pi". This step is then Blocked. Do not patch the kernel during the session.
5. When the shell is back: `lsusb` shows the stick if the cable stayed in. If the device was removed, there is a uevent, and plugging it again enumerates it. There is no sysfs path left for a dead device number that `lsusb` no longer shows.

The 500 ms figure after resume is the sheet target, and only when suspend actually ran. Record the number you measured.

Do not use `systemctl suspend` as a Pass condition of the host, storage, or HID items.
