# 05 — usbfs for user space

IDs: `USBDRV_SWREQ_005001` through `005018`

Do this now on Pi 4 with the USB stick already enumerated (item 01). `lsusb -v` reads the descriptor with a control transfer through usbfs. You do not need to send an AOA command. AOA and iAP2 commands belong to UsbManager, on this same node.

## Steps

Plug the stick. Take the bus and device numbers from `lsusb` (the Bus and Device columns):

```bash
lsusb
# example: Bus 002 Device 004: ID 0781:5581
BUS=002
DEVN=004
ls -l /dev/bus/usb/$BUS/$DEVN
```

The node part passes when this character device exists.

Read the descriptor from user space. Use root, because the product udev user is not decided yet:

```bash
sudo lsusb -v -s $BUS:$DEVN | tee "$LAB/usbfs-desc.txt" | head -n 40
```

Pass when the file contains `Device Descriptor`, `idVendor`, and `idProduct` matching `lsusb`. If `lsusb -v` reports a permission error or cannot open the device, the node is missing or usbfs is absent. On an image that already has a USB host, that is a Fail. If the kernel was built without USB core, it is Not enabled. With `CONFIG_USB`, this node comes with usbcore. Raspberry Pi OS does not need an extra option.

## Permissions other than root

See the mode:

```bash
stat -c '%A %U %G' /dev/bus/usb/$BUS/$DEVN
```

The lab may Pass the transfer part as root. The product rule waits on the service user/group (the open question on the sheet).

When you have a user to try, for example the `plugdev` group, this is a temporary Pi rule. It is not the product rule:

```bash
echo 'SUBSYSTEM=="usb", MODE="0660", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/99-usbdrv-lab.rules
sudo udevadm control --reload
sudo udevadm trigger --subsystem-match=usb
# unplug the stick and plug it again
stat -c '%A %U %G' /dev/bus/usb/$BUS/$DEVN
```

A user in `plugdev` runs `lsusb -v -s $BUS:$DEVN` without `sudo` and can read the descriptor. The lab permission part then passes.

Remove the lab rule when the shared machine should go back:

```bash
sudo rm /etc/udev/rules.d/99-usbdrv-lab.rules
sudo udevadm control --reload
```

Do not ship a `MODE="0666"` rule in the product image.

## Unplug during a transfer

`lsusb -v` is often too fast to unplug in time. Start it and unplug the stick immediately:

```bash
sudo lsusb -v -s $BUS:$DEVN
```

The command must finish, print an error, and return the shell. A reboot is not required. After that, `ls /dev/bus/usb/$BUS/$DEVN` does not exist.

If you cannot unplug in time because the command finishes first, write "did not unplug during the transfer" and take the unplug result from item 12. Do not Fail only because you were not fast enough.

## An interface already held by a driver

On a USB stick the storage interface already has a driver. Reading the **device** descriptor with `lsusb -v` must still work, because that transfer does not claim the interface. Do not detach `usb-storage` in this item.

Detach is what libusb does later when user space must claim an interface a class driver holds (`libusb_detach_kernel_driver`). Do not write a detach program in this session if `lsusb -v` already read the descriptor.

## Not enabled

Only when `/dev/bus/usb` does not exist while `lsusb` already sees the device. Check that the image has the USB core. Do not add another daemon in place of usbfs.
