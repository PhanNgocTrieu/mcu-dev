# 01 — Host controller and enumeration

IDs: `USBDRV_SWREQ_001001` through `001019`

On Pi 4 USB-A this is already present when `lsusb -t` shows `xhci_hcd`. A full Raspberry Pi OS image does not need a kernel change.

Equipment: a Pi 4 at a shell, one USB stick. A phone and an iPhone are extra steps.

## What this proves

1. At boot, with nothing plugged, a USB host bus already exists.
2. A healthy device plugged into USB-A gets a VID/PID, a sysfs entry, a `/dev/bus/usb/...` node, and an `add` uevent.
3. Unplug produces a `remove` uevent and the sysfs entry is gone.
4. USB-A does not become a gadget.

## Steps

Prepare the log directory from [README.md](README.md).

### 1. The host bus is up

```bash
lsusb
lsusb -t
```

Pass when `lsusb -t` has at least one tree with `Driver=xhci_hcd` and a `Class=root_hub` line. That is the USB-A port. Pi 4 usually shows two xHCI root hubs (one USB 2.0 bus and one USB 3.0 bus) of the same VL805 controller.

Paste the `lsusb -t` block into the report. On the EVB the driver name may differ. Replace the `xhci_hcd` criterion with the driver name from that board's device tree. A root hub is still required.

If there is no USB bus at all:

- Result: **Not enabled**.
- On an image you build, enable `CONFIG_USB`, `CONFIG_USB_XHCI_HCD`, and `CONFIG_USB_XHCI_PCI`, rebuild the kernel, boot, and repeat step 1.
- On Raspberry Pi OS, an empty result means stop. Keep the log. Do not continue to later items. Fix the boot image. Do not write a class driver.

### 2. Plug and enumerate

Terminal A:

```bash
sudo udevadm monitor --kernel --property --subsystem-match=usb
```

Terminal B:

```bash
echo "=== USBDRV MARK enum-in ===" | sudo tee /dev/kmsg >/dev/null
```

Plug the USB stick into a USB-A port. Wait about 2 seconds. Terminal B:

```bash
lsusb | tee "$LAB/lsusb-stick.txt"
lsusb -t | tee "$LAB/lsusb-t-stick.txt"
```

Terminal A must show an `add` uevent with:

- `ACTION=add`
- `SUBSYSTEM=usb`
- `DEVTYPE=usb_device`
- `PRODUCT=` containing three hex numbers: VID, PID, and bcdDevice

Press Ctrl-C on terminal A after the `add` line. Keep the file if you used `tee`.

Find sysfs. Replace VID and PID with the stick's `lsusb` line:

```bash
VID=0781
PID=5581
DEV=$(for d in /sys/bus/usb/devices/[0-9]-[0-9]*; do
  [ -f "$d/idVendor" ] || continue
  [ "$(cat "$d/idVendor")" = "$VID" ] && [ "$(cat "$d/idProduct")" = "$PID" ] && echo "$d" && break
done)
echo "DEV=$DEV"
echo "idVendor=$(cat "$DEV/idVendor") idProduct=$(cat "$DEV/idProduct")"
echo -n "serial="; cat "$DEV/serial" 2>/dev/null || echo "(no iSerial in the descriptor)"
BUS=$(cat "$DEV/busnum")
DNUM=$(printf "%03d" "$(cat "$DEV/devnum")")
ls -l "/dev/bus/usb/${BUS}/${DNUM}"
```

Pass when all four are true:

- `lsusb` shows the VID/PID you just plugged
- `idVendor` / `idProduct` match those two numbers
- the node `/dev/bus/usb/<bus>/<three digits>` exists
- the `add` uevent contains `PRODUCT=`

An empty `serial` is acceptable only when the device has no iSerial. Write that sentence in the report. The serial part still passes.

`dmesg` at the same time must contain a `New USB device found` line with the same id:

```bash
dmesg -T | awk 'f{print} /USBDRV MARK enum-in/{f=1}' | tee "$LAB/dmesg-enum.txt"
```

### 3. Unplug

Start `udevadm monitor` again if you stopped it. Unplug the stick.

Pass when:

- `lsusb` no longer shows that VID/PID
- `$DEV` is gone (`test ! -d "$DEV" && echo gone`)
- a uevent has `ACTION=remove` and `DEVTYPE=usb_device`

### 4. USB-A is not a gadget

While the stick is plugged in step 2, the stick sits under the `xhci_hcd` branch in `lsusb -t`, not under `dwc2`.

```bash
ls /sys/class/udc
```

An empty list is correct for a host-only image. If a UDC name is present and you have not done item 04, write "unexpected UDC" and check for a `dwc2` overlay in `config.txt`. The host item still passes if the stick is under `xhci_hcd`.

### 5. Phone and iPhone (extra, same enumeration criteria)

Plug an Android phone with a data cable so the phone shows USB on its screen. `lsusb` must show a new VID/PID. Save the descriptor:

```bash
sudo lsusb -v -d VVVV:PPPP > "$LAB/android-desc.txt"
```

The host item passes when the VID/PID exists and the descriptor file exists. MTP, charge-only, or no network does not fail this item.

If you have an iPhone and it has trusted the computer when iOS asked: `lsusb` must show `05ac`. Save `sudo lsusb -v -d 05ac: > "$LAB/apple-desc.txt"`. CarPlay on the screen is not required. With no iPhone, record Blocked for the Apple step only. The stick result stands.

## Do not record Fail for these

- Enumeration slower than 1 second because of an unpowered hub, or because the phone is asking to trust the computer. The 1 second figure on the sheet is a later measurement target for a stick plugged straight into the port. Record the time you saw. Do not fail the first lab session on it.
- An iPhone that only exposes a vendor class. Enumeration still passes when VID `05ac` is on `lsusb`.

## When it is not enabled

Only when step 1 has no bus, or `lsusb -t` has no host controller for the port you used. Add this to the image kernel fragment, then flash again:

```
CONFIG_USB=y
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_XHCI_PCI=y
```

On the EVB, replace the `XHCI_PCI` pair with the HCD symbol of that SoC (DWC3, EHCI, or platform xHCI). The symbol is chosen from the board device tree. Do not copy the Pi 4 overlay.
