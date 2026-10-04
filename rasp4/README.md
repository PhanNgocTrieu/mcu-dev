# rasp4 — Raspberry Pi 4 test host

Same `modules/` as `meter-pf`, two entry points:

| Folder | Purpose |
|---|---|
| `build-raspi4/` | Yocto Scarthgap image you flash to the Pi 4 |
| `build-demo/` | Virtual cluster + USB demo panel on the PC |

## Flashable image

```sh
./build-raspi4/build-image.sh
./build-raspi4/publish-image.sh   # copy ~90MB .wic.bz2 → images/pi4/ (có thể commit)
./build-raspi4/flash-sd.sh /dev/sdX
```

Cây Yocto (`build/`, `downloads/`) rất nặng (hàng chục GB) — **không** commit. Chỉ artifact flash trong [../images/](../images/README.md).

## Virtual monitors

```sh
./build-demo/scripts/run-host-sim.sh
```

stderr shows `usb-driver` / `usb-man` TRACE logs (plug, AOA, media, touch).
On the board: `journalctl -u usb-driverd -u usb-managerd -f`.
