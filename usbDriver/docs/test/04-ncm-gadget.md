# 04 — NCM gadget (configfs)

IDs: `USBDRV_SWREQ_004001` through `004018`

This is the direction where **the head unit is the USB device** and an iPhone or a PC is the USB host. It is not [08-cdc-ncm-host.md](08-cdc-ncm-host.md) (the Pi is the host and the phone is the device).

On a host-only Pi 4 image this is **Not enabled**. `ls /sys/class/udc` is empty. Do not record Fail.

Run the stand-in after [02-dual-role.md](02-dual-role.md) level B, when `/sys/class/udc` has a name. The other computer is a stand-in host, in place of an iPhone. A CarPlay session is not a Pass criterion.

Qualification on the EVB phone port counts only after the role is `device` from item 02 level C.

## Check before creating the gadget

On the Pi:

```bash
ls /sys/class/udc
ls /sys/kernel/config 2>/dev/null || echo "no configfs"
modprobe libcomposite && echo libcomposite-ok || echo "no libcomposite"
zcat /proc/config.gz 2>/dev/null | grep -E 'CONFIG_USB_CONFIGFS|CONFIG_USB_LIBCOMPOSITE|CONFIG_USB_GADGET='
```

You need:

- one UDC name (after the `dwc2` overlay, or on the EVB after role = device)
- `libcomposite` loads
- configfs, mounted in the steps below

No UDC: go back to item 02 level B. Do not continue.

Missing `libcomposite` or `functions/ncm` means the kernel has no gadget support. Record **Not enabled** and add these lines to the kernel fragment for the next image. Do not write a new NCM function:

```
CONFIG_USB_GADGET=y
CONFIG_USB_LIBCOMPOSITE=m
CONFIG_USB_CONFIGFS=m
CONFIG_USB_CONFIGFS_NCM=y
```

On some kernel trees `CONFIG_USB_CONFIGFS_NCM` is a bool tied to `CONFIG_USB_CONFIGFS`. If `olddefconfig` drops that line, enable `CONFIG_USB_CONFIGFS_F_NCM`, or select Network Control Model in the gadget menu, then rebuild. Take the symbol name from `make menuconfig` of the kernel you actually boot, and write the symbol you enabled into the report.

On Raspberry Pi OS, `libcomposite` is often already a module once the dwc2 overlay is up. Try `sudo modprobe libcomposite` before rebuilding the whole kernel.

## Create the NCM gadget

Run this on the Pi after a UDC exists. VID `1d6b` PID `0104` is a lab id (Linux Foundation, gadget Ethernet). Do not change it to the Apple VID.

```bash
set -e
sudo modprobe libcomposite
sudo mount -t configfs none /sys/kernel/config 2>/dev/null || true
cd /sys/kernel/config/usb_gadget
sudo mkdir -p g_ncm
cd g_ncm
echo 0x1d6b | sudo tee idVendor
echo 0x0104 | sudo tee idProduct
echo 0x0200 | sudo tee bcdUSB
sudo mkdir -p strings/0x409
echo 0123456789 | sudo tee strings/0x409/serialnumber
echo Lab | sudo tee strings/0x409/manufacturer
echo NCM-gadget | sudo tee strings/0x409/product
sudo mkdir -p configs/c.1/strings/0x409
echo NCM | sudo tee configs/c.1/strings/0x409/configuration
echo 250 | sudo tee configs/c.1/MaxPower
sudo mkdir -p functions/ncm.usb0
echo "ifname=$(cat functions/ncm.usb0/ifname)"
echo "qmult=$(cat functions/ncm.usb0/qmult)"
sudo ln -s functions/ncm.usb0 configs/c.1/
UDC=$(ls /sys/class/udc | head -n 1)
echo "bind $UDC"
echo "$UDC" | sudo tee UDC
ip -br link show | tee "$HOME/ncm-link.txt"
```

The create step passes when:

- `tee UDC` prints no error
- `ip -br link` shows the function's interface. The default `ifname` pattern is `ncm%d`, which usually becomes `ncm0`. Record the name you actually see. If udev renames it to `enx…` or `usb0`, match it with `ip -d link` against the interface that appeared after bind. Do not fail on the name alone.
- On the **computer that is the host** (the machine whose USB cable is plugged into the Pi): `lsusb` shows `1d6b:0104`. That is enumeration on the host side.

```bash
# run on the PC host, not on the Pi
lsusb -d 1d6b:0104
```

Carrier on the Pi may stay down until the PC sets the configuration. On the Pi:

```bash
ip -br link
cat /sys/class/net/ncm0/carrier 2>/dev/null || true
```

The driver criterion is a successful UDC bind and an interface that exists. Carrier, once the PC has accepted the device, is extra. Record it. An IP address is not required.

## Bad attribute

Do this **before** bind on a later run, or after teardown below. Write a bad MAC:

```bash
echo zz | sudo tee /sys/kernel/config/usb_gadget/g_ncm/functions/ncm.usb0/host_addr
```

Pass when the command returns an error (exit status is not 0) and the gadget does not enter a strange state. Change attributes only while unbound. If it is already bound, clear the UDC first.

## Remove the gadget

```bash
cd /sys/kernel/config/usb_gadget/g_ncm
echo "" | sudo tee UDC
sudo rm configs/c.1/ncm.usb0
sudo rmdir functions/ncm.usb0
sudo rmdir configs/c.1/strings/0x409
sudo rmdir configs/c.1
sudo rmdir strings/0x409
cd ..
sudo rmdir g_ncm
ip -br link | tee "$HOME/ncm-link-after.txt" || true
```

Pass when the ncm interface you created is gone and the PC no longer shows `1d6b:0104`.

If `rmdir` says the directory is busy, the order is wrong: clear the `UDC` file first, remove the symlink `configs/c.1/ncm.usb0`, then remove `functions/ncm.usb0`.

## Write an attribute after bind

Create the gadget again through the `tee UDC` step, then:

```bash
echo 10 | sudo tee functions/ncm.usb0/qmult; echo exit:$?
```

Pass when the write is rejected (`-EBUSY`, or `tee` fails) because the gadget is already bound.

## Do not treat these as Pass

- CarPlay on an iPhone screen.
- A `cdc_ncm` interface while the Pi is the host (that is item 08).
- A result taken while the Pi dwc2 overlay is on, if the EVB product port has not run the same steps.

When the stand-in session is over, restore `config.txt` as in item 02 so the other host tests can use normal USB-C power again.
