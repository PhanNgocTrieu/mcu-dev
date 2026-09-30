# 02 — Dual-Role (host to device)

IDs: `USBDRV_SWREQ_002001` through `002019`

Pi 4 USB-A: **Not enabled**, and it cannot be enabled. USB-A goes through xHCI and is host-only. Record **Not enabled** for the feature port. Do not record Fail because `/sys/class/usb_role` is missing.

Three levels:

- Level A — do it now, on the image you are running, without changing boot files. The goal is to show that USB-A stays a host.
- Level B — USB-C stand-in, only when you deliberately enable `dwc2`. A level B result is not proof of the EVB phone port.
- Level C — EVB, when the board has a DRD controller. This is the qualification of `USBDRV_SWREQ_002xxx`.

## Level A — do it now

```bash
lsusb -t
ls -d /sys/class/usb_role/* 2>/dev/null || echo "no usb_role"
ls /sys/class/udc 2>/dev/null || echo "no udc"
```

Level A passes when a stick or mouse on USB-A is still under `xhci_hcd` (item 01 already passed) and you do not write a role to the USB-A controller. No `usb_role` directory is the correct result on a default Pi 4.

Report line: `02 Dual-Role on USB-A: Not enabled`.

## Level B — USB-C stand-in (development, not a substitute for EVB)

Do this only when you need to practice configfs before the EVB exists, together with [04-ncm-gadget.md](04-ncm-gadget.md).

### Power and cable

1. Power the Pi off.
2. Feed 5V and GND to the Pi 4 header from a 5V supply that can provide enough current. Do not also plug a charger into USB-C during this.
3. The USB-C cable from the Pi's USB-C port to a USB-A port on the other computer must carry data.

### Turn on the USB-C controller

See which boot file exists:

```bash
ls /boot/firmware/config.txt /boot/config.txt 2>/dev/null
```

Call the file you found `CONFIG_TXT`. Bookworm uses `/boot/firmware/config.txt`.

```bash
sudo cp "$CONFIG_TXT" "$CONFIG_TXT.bak-usbdrv"
echo "dtoverlay=dwc2,dr_mode=peripheral" | sudo tee -a "$CONFIG_TXT"
sudo reboot
```

After reboot, SSH or the console must still come up (Ethernet or UART). If it does not boot: power off, edit the card on another machine, delete the line you added, or copy `.bak-usbdrv` back.

When the shell is back:

```bash
ls /sys/class/udc
```

The "device controller exists" part passes when this prints a name, usually containing `dwc2` or the address `fe980000.usb`.

Standard role switch:

```bash
ls /sys/class/usb_role 2>/dev/null || echo "Pi 4 does not create usb_role with the peripheral overlay"
```

`dr_mode=peripheral` locks the device role, so **no** `host`/`device` role file is a common result on Pi 4. Write that sentence down. It is not a failure of the `usb_role` class on the EVB, because this overlay is not DRD.

To try a role change on the Pi, change the overlay line to:

```text
dtoverlay=dwc2,dr_mode=otg
```

reboot, then:

```bash
ls -l /sys/class/usb_role/*/role
cat /sys/class/usb_role/*/role
echo device | sudo tee /sys/class/usb_role/*/role
cat /sys/class/usb_role/*/role
echo host | sudo tee /sys/class/usb_role/*/role
cat /sys/class/usb_role/*/role
```

- If there is no `role` file: record **Not enabled** on Pi 4 (the USB-C ID/OTG pin does not implement a head-unit role switch). Stop level B. Do not patch the kernel during the session.
- If writing `device` reads back `device` and `/sys/class/udc` has a name: record "role stand-in observed", with the log. Do not sign `USBDRV_SWREQ_002xxx` as Pass for the product port.
- Writing `xyz` must fail and the role must not change:

```bash
echo xyz | sudo tee /sys/class/usb_role/*/role ; echo exit:$?
cat /sys/class/usb_role/*/role
```

When the stand-in is finished, restore boot:

```bash
sudo cp "$CONFIG_TXT.bak-usbdrv" "$CONFIG_TXT"
sudo reboot
```

Power from USB-C as usual only after the overlay is gone and the header supply is disconnected if you are not using both.

## Level C — EVB (when the board exists)

Do these in order. Write the device-tree node name into the report.

1. In the phone-port DTS: `dr_mode = "otg"` and `usb-role-switch`. The mode after boot is host: a USB stick enumerates (item 01).
2. Find the role file:

```bash
ls -l /sys/class/usb_role/
cat /sys/class/usb_role/*role*/role
```

It reads `host`.

3. Write device:

```bash
echo device | sudo tee /sys/class/usb_role/<exact-name>/role
cat /sys/class/usb_role/<exact-name>/role
ls /sys/class/udc
```

Pass when the role reads back `device` and `ls /sys/class/udc` contains that port's controller. A device that was plugged on this port in host mode must disappear, with a remove uevent (how to watch uevents is in the README).

4. Attach the NCM gadget from item 04, then write host again:

```bash
echo host | sudo tee /sys/class/usb_role/<exact-name>/role
```

Pass when the role is `host`, that port's UDC is not left bound to a gadget, and plugging the stick again makes `lsusb` show it.

5. Time it with a `kmsg` mark immediately before `echo device`:

```bash
echo "=== USBDRV MARK role ===" | sudo tee /dev/kmsg >/dev/null
echo device | sudo tee /sys/class/usb_role/<exact-name>/role
```

The 100 ms figure on the sheet is an EVB measurement target. On the first session, record the number you measured. Do not call it Fail only because it was slower, while the role value is correct, until a signed limit exists.

6. VBUS while role = device: the phone stays powered. How you measure that depends on the circuit (a meter, or the phone still showing charge). Without a schematic this step is Blocked. Do not invent a GPIO number.

If the BSP only has a vendor sysfs node (no `/sys/class/usb_role`), record the open question on sheet item 2 (Q2) and put the vendor path in the report. UsbManager is not wired to that path until the `usb_role` class is chosen.

## What to build if the EVB has no role switch yet

- Enable the DRD config of the right IP (DWC3 dual-role or DWC2 dual-role) and USB gadget in the board kernel.
- Edit the device tree: `dr_mode = "otg"`, `usb-role-switch`.
- Prefer an upstream driver that exposes the `usb_role` class. Write a small wrapper only when the BSP has no such class, and that wrapper must accept the same two values, `host` and `device`.
- Do not set `dr_mode=otg` on Pi 4 USB-A.
