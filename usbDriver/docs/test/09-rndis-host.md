# 09 — RNDIS host

IDs: `USBDRV_SWREQ_009001` through `009018`

The Pi is the USB host. The right sample is an Android phone with USB tethering on. An IP address is not required and a ping is not required.

An iPhone is not an RNDIS sample. Do not use an iPhone to judge this item.

## On the phone

1. Plug the data cable into a USB-A port on the Pi.
2. Turn on USB tethering (hotspot over USB). Some phones need the USB mode set to tethering, not "charge only" or "file transfer".
3. The phone may re-enumerate. Wait 3 seconds.

## Steps

```bash
ip -br link | tee "$LAB/ip-before-rndis.txt"
echo "=== USBDRV MARK rndis ===" | sudo tee /dev/kmsg >/dev/null
# enable tether or replug if needed
sleep 3
sudo modprobe rndis_host || true
lsusb | tee "$LAB/lsusb-rndis.txt"
```

Take the phone VID/PID and save the descriptor:

```bash
sudo lsusb -v -d VVVV:PPPP > "$LAB/android-rndis-desc.txt"
```

Find `DEV` in sysfs (README).

```bash
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo none; fi
done
ip -br link | tee "$LAB/ip-rndis.txt"
dmesg -T | awk 'f{print} /USBDRV MARK rndis/{f=1}' | tee "$LAB/dmesg-rndis.txt"
```

Pass when one interface driver is `rndis_host` and `ip -br` shows a new network interface.

Turn tethering off or unplug the cable. The interface disappears. `lsusb` no longer shows that device, or the phone returns to a different MTP PID — record the new PID. The RNDIS interface must be gone.

## If rndis_host does not appear

- The descriptor has no RNDIS interface and `dmesg` has no `rndis` line: the phone is not tethered. Record **Blocked**, turn tethering on, and repeat. MTP is not a Fail.
- The descriptor has RNDIS (often Wireless class 224, subclass 1, protocol 3) and `modprobe rndis_host` says the module is missing: **Not enabled**. Add these and rebuild:

```
CONFIG_USB_USBNET=m
CONFIG_USB_NET_RNDIS_HOST=m
```

- Another driver holds the interface (`cdc_ether`, `cdc_ncm`): record the real driver and subclass. If it is NCM, this item is Blocked and item 08 takes that sample.
