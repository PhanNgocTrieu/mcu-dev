# HUPI module-usb

Separated USB modules (no more single “connectivity” blob), shared by the EVB
and the Raspberry Pi 4 lab.

```text
modules/
  usb-driver-userlayer/   # plug/unplug, enumerate, usbfs lease
  usb-man/                # session policy, AOA, one backend
  libhu-aa/               # Android Auto media (AASDK-ready)
  libhu-carplay/          # CarPlay media over NCM (MFi-ready)
  common/                 # wire protocol + tracing logger
meta-connectivity/        # Yocto layer (recipes-conn-app / recipes-conn-lib)
meter-pf/                 # EVB main board configs
rasp4/
  build-raspi4/           # real flashable image
  build-demo/             # virtual monitors
tools/
```

## Logs

Daemons default to **trace** on the demo path and log every plug/unplug,
claim, AOA step, session state, media start/stop, and touch:

```sh
./rasp4/build-demo/scripts/run-host-sim.sh
# or on the board:
journalctl -u usb-driverd -u usb-managerd -f
```

`--log-level error|warn|info|trace` is accepted by both daemons.

## Media

| Backend | Library | Board flag |
|---|---|---|
| Android Auto | `libhu-aa` | `-DHUPI_WITH_AASDK=ON` + `third_party/aasdk` |
| CarPlay | `libhu-carplay` | `-DHUPI_WITH_MFI=ON` + libiap2/libhu-mfi |

Without those trees the libraries still emit traced video frames so graphics
and demos keep working; enable the flags on `meter-pf` for the production image.

## Build

```sh
# Host unit tests + demos
cmake -S . -B build -DHUPI_HOST_SDL=ON && cmake --build build && ctest --test-dir build

# Pi 4 flashable image
./rasp4/build-raspi4/build-image.sh
```
