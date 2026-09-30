# USB driver tests on Raspberry Pi 4

These steps check the USB driver layer (standard Linux) on the lab board. The **host** proof port is a **USB-A** port on Raspberry Pi 4 (xHCI, VL805). The same commands apply on the EVB once that board boots Linux and the USB host port is up. The host-controller name may differ (`xhci_hcd` on Pi 4, another driver on the EVB).

Requirements are in:

`usbDriver/SoftChngReqAnalSh_Connectivity_UsbDriver.xlsx`

sheet `(1)2-1.Req Analysis - New`.

The original file in Downloads could not be overwritten. The English copy is:

`SoftChngReqAnalSh_Connectivity_UsbDriver_EN.xlsx`

## Result words

Use one of these four words:

| Word | Meaning |
|---|---|
| Pass | The steps were done and the criteria of that item are met |
| Fail | The feature is enabled, the right sample is present, and the result is wrong |
| Not enabled | The kernel or the board does not have this feature yet. This is not a defect of the driver that is running |
| Blocked | The sample is missing (no stick, phone tethering off, no EVB). Do not record Pass |

Dual-Role, the NCM gadget, and the Meter VBUS GPIO are **Not enabled** on USB-A. The stand-in steps are in those files. A stand-in result does not replace proof on the EVB product port.

## Suggested order

1. [01-host-enumeration.md](01-host-enumeration.md) first. Later items need enumeration to pass.
2. [07-mass-storage.md](07-mass-storage.md), [11-hid.md](11-hid.md), [12-unplug.md](12-unplug.md) need only a USB stick and a mouse or keyboard.
3. [05-usbfs.md](05-usbfs.md) and [06-runtime-pm.md](06-runtime-pm.md) use that same stick.
4. [08-cdc-ncm-host.md](08-cdc-ncm-host.md) and [09-rndis-host.md](09-rndis-host.md) need a phone with USB tethering on.
5. [10-acm-serial.md](10-acm-serial.md) needs a CDC-ACM device or a USB-UART dongle.
6. [03-vbus-overcurrent.md](03-vbus-overcurrent.md): enumeration proves VBUS. Over-current and the GPIO come later.
7. [02-dual-role.md](02-dual-role.md) and [04-ncm-gadget.md](04-ncm-gadget.md) are not on USB-A. Do them when you deliberately enable USB-C, or when the EVB exists.

## On the bench

- Raspberry Pi 4, power cable, a card that already boots (Raspberry Pi OS or a Yocto image that reaches a shell).
- One USB stick that can still be read. Do not use a stick whose data must not be touched. The test only reads. It does not format.
- A USB mouse or keyboard.
- An Android phone and a data cable (not a charge-only cable).
- An iPhone is optional, only to see VID `05ac` in the host item. CarPlay projection is not required.
- A second computer and a USB-C data cable only for the NCM gadget item. For that item the Pi is powered from the 5V header, not from the USB-C port that is carrying data.

Plug the stick, mouse, and phone into **USB-A**. Do not plug a host-test sample into USB-C.

## Where logs go

One directory per session. On the Pi:

```bash
LAB=~/usbdrv-lab/$(date +%Y%m%d-%H%M)
mkdir -p "$LAB"
cd "$LAB"
uname -a | tee uname.txt
tr -d '\0' < /proc/device-tree/model 2>/dev/null | tee model.txt; echo
lsusb | tee lsusb-boot.txt
lsusb -t | tee lsusb-t-boot.txt
echo "$LAB"
```

Keep the `tee` files. When an item is finished, add one line to `ket-qua.txt`:

```text
01 host     Pass        lsusb-t-boot.txt
07 storage  Pass
02 role     Not enabled
```

## Commands used in more than one test

Mark the kernel log, then plug or unplug:

```bash
echo "=== USBDRV MARK $(date +%T) ===" | sudo tee /dev/kmsg >/dev/null
# do the action (plug, unplug, sysfs write)
sleep 2
dmesg -T | awk 'f{print} /USBDRV MARK/{f=1}' | tee "$LAB/dmesg-mark.txt"
```

`dmesg -T` needs permission to read the kernel log. If it says permission denied, add `sudo`. Do not use `dmesg -c` (that command clears the log).

Watch kernel uevents in a terminal and leave it running:

```bash
sudo udevadm monitor --kernel --property --subsystem-match=usb | tee "$LAB/uevent.txt"
```

Find the sysfs directory of a device you just saw in `lsusb`. Replace `0781` and `5581` with the VID and PID on the `lsusb` line (lowercase, no `0x`):

```bash
VID=0781
PID=5581
for d in /sys/bus/usb/devices/[0-9]-[0-9]*; do
  [ -f "$d/idVendor" ] || continue
  if [ "$(cat "$d/idVendor")" = "$VID" ] && [ "$(cat "$d/idProduct")" = "$PID" ]; then
    echo "$d"
  fi
done
```

The path looks like `/sys/bus/usb/devices/2-1` (`bus-port`). Interfaces are `2-1:1.0`, `2-1:1.1`, and so on.

Show the driver of each interface:

```bash
DEV=/sys/bus/usb/devices/2-1   # path printed above
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo "(no driver)"; fi
done
echo -n "usbfs node: "
ls -l /dev/bus/usb/$(cat "$DEV/busnum")/$(printf "%03d" $(cat "$DEV/devnum"))
```

## Kernel config of the running image

On many Pi images:

```bash
sudo modprobe configs 2>/dev/null || true
if [ -r /proc/config.gz ]; then
  zcat /proc/config.gz | grep -E 'CONFIG_USB=|CONFIG_USB_XHCI|CONFIG_USB_STORAGE|CONFIG_USB_ACM|CONFIG_USB_SERIAL|CONFIG_USB_NET_|CONFIG_USB_HID|CONFIG_USB_GADGET|CONFIG_USB_CONFIGFS|CONFIG_USB_DWC2|CONFIG_USB_DWC3' | tee "$LAB/usb-config.txt"
else
  echo "No /proc/config.gz — use lsmod, lsusb -t, and the sysfs driver link"
fi
```

The project image fragment (`connectivity-usb.cfg` in the module plan) needs these host lines:

```
CONFIG_USB=y
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_XHCI_PCI=y
CONFIG_USB_STORAGE=m
CONFIG_USB_ACM=m
CONFIG_USB_SERIAL=m
CONFIG_USB_NET_DRIVERS=m
CONFIG_USB_USBNET=m
CONFIG_USB_NET_CDCETHER=m
CONFIG_USB_NET_CDC_NCM=m
CONFIG_USB_NET_RNDIS_HOST=m
CONFIG_USB_HID=y
```

A full Raspberry Pi OS image usually already has these host items. If a module is missing, record **Not enabled** for that item and follow the "enable" section in that item's file. Do not write a new driver when upstream already has one and only the config is missing.

## Safety

- Do not short 5V to ground. Do not poke a pin into a USB port to "create over-current".
- Do not format the stick. Do not run `mkfs` or a `dd` that overwrites it.
- Before editing `/boot/firmware/config.txt` or `/boot/config.txt` for the gadget item, copy a backup. If the Pi does not boot, mount the card on another machine and restore the backup.
- When USB-C is used as a gadget, power the Pi from the 5V header (a stable 5V supply, enough current) and GND. Do not feed 5V from USB-C at the same time as the header.

## Test files and sheet IDs

| File | Sheet function | Main IDs |
|---|---|---|
| 01 | 1 Host / enumeration | `USBDRV_SWREQ_001xxx` |
| 02 | 2 Dual-Role | `USBDRV_SWREQ_002xxx` |
| 03 | 3 VBUS / over-current | `USBDRV_SWREQ_003xxx` |
| 04 | 4 NCM gadget | `USBDRV_SWREQ_004xxx` |
| 05 | 5 usbfs | `USBDRV_SWREQ_005xxx` |
| 06 | 6 runtime PM | `USBDRV_SWREQ_006xxx` |
| 07 | 7 mass storage | `USBDRV_SWREQ_007xxx` |
| 08 | 8 CDC-NCM host | `USBDRV_SWREQ_008xxx` |
| 09 | 9 RNDIS host | `USBDRV_SWREQ_009xxx` |
| 10 | 10 ACM / serial | `USBDRV_SWREQ_010xxx` |
| 11 | 11 HID | `USBDRV_SWREQ_011xxx` |
| 12 | 12 unplug | `USBDRV_SWREQ_012xxx` |

The older seed IDs `USBDRV-REQ-001` through `017` in `ASPICE-PLAN.md` cover the host side only. This sheet is the RA after Dual-Role, VBUS, the NCM gadget, and PM were added. Where the two disagree, the `USBDRV_SWREQ_*` IDs on the sheet are the requirements under analysis.
