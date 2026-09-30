# 10 — CDC-ACM and USB serial

IDs: `USBDRV_SWREQ_010001` through `010018`

Two different devices:

- CDC-ACM (many boards in the Arduino style, USB modems with class 02/02): node `/dev/ttyACM*`, driver `cdc_acm`. The fragment already has `CONFIG_USB_ACM`. Do this if you have that kind of device.
- A USB-UART chip (FTDI, CH340, CP2102, PL2303): node `/dev/ttyUSB*` only when **that chip's** driver is enabled. `CONFIG_USB_SERIAL=m` alone is not enough. On Raspberry Pi OS the chip drivers are usually already modules. On a Yocto image that matches the current fragment, the chip drivers are absent: record **Not enabled**, then enable the symbol of the chip you have. Do not write a new driver if upstream already has one.

A phone in ADB mode does not create a tty for this item. It is neither Pass nor Fail.

If you have neither an ACM device nor a USB-UART dongle, record Blocked — missing sample.

## CDC-ACM

```bash
ls /dev/ttyACM* 2>/dev/null | tee "$LAB/tty-before.txt" || true
echo "=== USBDRV MARK acm ===" | sudo tee /dev/kmsg >/dev/null
# plug the ACM device into USB-A
sleep 2
ls -l /dev/ttyACM* | tee "$LAB/tty-acm.txt"
```

There is a node that was not there before. Take `DEV` from VID/PID.

```bash
for i in "$DEV":*; do
  echo -n "$i -> "
  if [ -L "$i/driver" ]; then basename "$(readlink -f "$i/driver")"; else echo none; fi
done
```

Pass when the driver is `cdc_acm` and a new `/dev/ttyACM*` node exists.

Open it. The device does not have to send bytes:

```bash
sudo stty -F /dev/ttyACM0 115200
echo open-ok
```

Pass when the command does not report "No such device".

Unplug. That node disappears.

If `modprobe cdc_acm` says "not found" while the descriptor is ACM (class 2, subclass 2): Not enabled. Add `CONFIG_USB_ACM=m` and rebuild.

## USB serial by chip

Plug the dongle. Look at `lsusb` (for example `0403:6001` is FTDI, `1a86:7523` is CH340, `10c4:ea60` is CP210x).

```bash
lsusb | tee "$LAB/lsusb-uart.txt"
sudo modprobe ftdi_sio 2>/dev/null || true
sudo modprobe ch341 2>/dev/null || true
sudo modprobe cp210x 2>/dev/null || true
sudo modprobe pl2303 2>/dev/null || true
sleep 1
ls -l /dev/ttyUSB* 2>/dev/null || echo "no ttyUSB yet"
dmesg | tail -n 40 | tee "$LAB/dmesg-uart.txt"
```

Pass when `/dev/ttyUSB*` appears and the interface driver matches the chip (`ftdi_sio`, `ch341`, `cp210x`, or `pl2303`).

If dmesg never binds a driver and `modprobe` says the module is missing: **Not enabled** for that chip. Add one line and rebuild. Do not add all four if the lab has only one chip:

```
CONFIG_USB_SERIAL=m
CONFIG_USB_SERIAL_FTDI_SIO=m
# or CONFIG_USB_SERIAL_CH341=m
# or CONFIG_USB_SERIAL_CP210X=m
# or CONFIG_USB_SERIAL_PL2303=m
```

Write the chip and the symbol you enabled into the report. The open question on the sheet is the list of chips the product fragment must keep.

Unplug the dongle. The matching `/dev/ttyUSB*` node disappears.
