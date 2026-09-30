# 08 — CDC-NCM host

IDs: `USBDRV_SWREQ_008001` through `008018`

The Pi is the USB host. The other device must be declaring CDC-NCM. This is not [04-ncm-gadget.md](04-ncm-gadget.md).

An IP address is not required. A ping is not required.

## Which sample to use

| What is on the wire | What to do |
|---|---|
| NCM subclass | Continue this item |
| RNDIS only (typical Android USB tethering) | This item is Blocked — no NCM sample. Do item 09 |
| ECM only | Record driver `cdc_ether` if it binds. This NCM item is Blocked |
| MTP / charge-only / ADB | There is no NCM. Turn tethering on or change the sample. Do not record a driver fail |

See the class after the phone is plugged:

```bash
sudo lsusb -v -d VVVV:PPPP | grep -E 'bInterfaceClass|bInterfaceSubClass|bInterfaceProtocol|iInterface'
```

NCM is `bInterfaceClass` 2 (Communications) and `bInterfaceSubClass` 13 (the decimal value of `0x0d`).

## Turn on Android tethering when you want to try

Settings → Network / Hotspot → USB tethering, with the cable already plugged. Many Android phones expose RNDIS, not NCM. If `lsusb -v` has no subclass 13, stop this item as Blocked and go to item 09.

An iPhone personal hotspot sometimes exposes NCM and sometimes an Apple vendor interface. If subclass 13 is absent, do not use the iPhone to pass this item.

A more reliable NCM source, if you have a second Linux machine already set up as an NCM gadget, is to plug that machine into the Pi's USB-A port. That machine is optional.

## Steps once subclass NCM is visible

```bash
ip -br link | tee "$LAB/ip-before.txt"
sudo modprobe cdc_ncm || true
echo "=== USBDRV MARK ncm ===" | sudo tee /dev/kmsg >/dev/null
# plug the NCM device, or enable tether and wait for re-enumeration
sleep 3
dmesg -T | awk 'f{print} /USBDRV MARK ncm/{f=1}' | tee "$LAB/dmesg-ncm.txt"
```

Find sysfs `DEV` from VID/PID (README).

```bash
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo none; fi
done
ip -br link | tee "$LAB/ip-ncm.txt"
```

Pass when one interface driver is `cdc_ncm` and `ip -br` shows a network interface that was not there before the plug.

Unplug the cable. That interface disappears.

## Not enabled

`lsusb -v` already shows NCM subclass, there is no driver, and:

```bash
sudo modprobe cdc_ncm
```

says the module does not exist. Add these to the fragment and rebuild:

```
CONFIG_USB_NET_DRIVERS=m
CONFIG_USB_USBNET=m
CONFIG_USB_NET_CDC_NCM=m
```

Do not rewrite `cdc_ncm`. ECM is on the same image if you also need it:

```
CONFIG_USB_NET_CDCETHER=m
```
