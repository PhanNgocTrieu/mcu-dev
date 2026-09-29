# usb-manager-c

C implementation of the USB Host manager. The C++ tree in `usb-manager/` is unchanged.

Application code is C11. D-Bus is the only C++ translation unit (`src/dbus_sdbuspp.cpp`) because [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) has no C API. That file exposes `dbus_api.h` as an `extern "C"` boundary; `main.c` never includes C++.

## Build

```bash
export USB_MANAGER_DEPS_PREFIX="$PWD/../.deps/prefix"   # from repo root, or this script
./scripts/build-c.sh
```

From the repository root:

```bash
./scripts/build-c.sh
./build-c/usb-manager-c-selftest
./build-c/usb-manager-c --no-dbus --debug
```

Needs `libudev`, `libusb-1.0`, and `libsdbus-c++` (pkg-config name `sdbus-c++`).

## Binaries

| Binary | Role |
|--------|------|
| `usb-udev-lab-c` | Plug/unplug lab |
| `usb-manager-c` | Daemon (sdbus-c++ IPC) |
| `usb-manager-c-selftest` | Classifier + exclusive policy |
