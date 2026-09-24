# Learning path — USB Manager / Connectivity SC

## 1. USB Host hotplug

- Run `udevadm monitor --subsystem-match=usb --property` while plugging a phone.
- Run `./build/usb-udev-lab` and compare VID/PID + `type=` output.
- Read `src/hotplug/UdevMonitor.cpp` and `src/classify/DeviceClassifier.cpp`.

## 2. Transports

- AOAP: `src/aoap/TransportProber.cpp` (`GET_PROTOCOL`).
- Network: look for `cdc_ncm` / `rndis_host` under `/sys/class/net/*/device/driver`.
- Optional: `usb_modeswitch` when a phone enumerates as storage first.

## 3. Session + policy

- State machine: `src/session/SessionOrchestrator.cpp`
- Exclusive AA/CarPlay: `src/policy/ConnectionPolicy.cpp`
- CarPlay stub always returns `MFI_REQUIRED` (production needs Apple MFi + licensed stack).

## 4. IPC

- D-Bus interface in `src/ipc/UsbDbusService.cpp`
- From a desktop session: `busctl --user introspect org.example.connectivity /org/example/connectivity/usb`

## 5. Yocto

- Layer: `meta-connectivity`
- Image: `core-image-connectivity`
- Kernel fragment: `connectivity-usb.cfg`
