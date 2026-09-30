# 03 — VBUS and over-current

IDs: `USBDRV_SWREQ_003001` through `003018`

On Pi 4 you can prove the hub part: USB-A has power because a device enumerates, and the over-current counter file exists. You cannot prove the Meter `vbus_on` / `vbus_off` GPIO, VBUS held across a role switch, or a deliberate over-current event.

Do not short the port. Do not wire 5V to GND.

## Do this now

A USB stick from item 01 that enumerated on USB-A is the proof that port VBUS was ON after boot. Use the same `lsusb` log as item 01.

Read the over-current counters. Plugged or unplugged both work:

```bash
find /sys/bus/usb/devices -name over_current_count -print -exec cat {} \;
```

Each hub port has one file. The value is how many times the hub has reported over-current since boot. On a Pi it is usually `0`.

The lab part passes when:

- A device plugged into USB-A enumerates (host VBUS was on at boot).
- `over_current_count` can be read. A value of `0` is valid when no event has happened.
- You did not create a short circuit.

Report:

```text
03 VBUS host on USB-A: Pass (device enumerated)
03 Meter VBUS GPIO: Not enabled (no EVB)
03 stimulate over-current: cannot be stimulated on Pi
```

## Do not do this on the Pi

- Holding VBUS when the role becomes device depends on item 02 level C. USB-A has no role switch, so this part is Not enabled.
- The GPIO name, and whether charge current is 500 mA or 1.5 A, stay blank until the schematic exists. Do not write a Pi header GPIO number into the EVB requirement.

When the EVB has the circuit:

1. Write the regulator or GPIO name from the device tree into the report (read it from the built DTS, do not guess).
2. Boot in host mode, plug a phone, and confirm enumeration.
3. Switch the role to device (item 02). The phone must still be powered. If power drops at the moment of the role write, this item Fails on the EVB.
4. Stimulate over-current only by a method the circuit was designed for (a test load that was calculated). Expected: `over_current_count` increases by 1, `dmesg` contains `over-current`, and `lsusb` loses the device on that port. After the kernel cooldown, the port may supply power again. Plugging the device again enumerates it.

Keep `over_current_count` before and after, and the `dmesg` section starting at `over-current`.
