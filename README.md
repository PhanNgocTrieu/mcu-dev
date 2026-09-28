# USB Manager — Connectivity SC Prototype (RPi4 Host)

C++ daemon that manages USB Host hotplug, device classification, Android Auto demo sessions (AASDK boundary), and a CarPlay stub for learning on Raspberry Pi 4 + Yocto.

## Layout

```
usb-manager/           # Daemon C++ (CMake)
usb-manager-c/         # Daemon C; D-Bus via sdbus-c++ only
meta-connectivity/     # Yocto layer (recipe, image, kernel cfg)
docs/                  # Learning notes + AASDK integration
```

## Quick build (dev host)

If system `-dev` packages are missing, use the extracted prefix under `.deps/prefix`:

```bash
export USB_MANAGER_DEPS_PREFIX="$PWD/.deps/prefix"
export PATH="$USB_MANAGER_DEPS_PREFIX/usr/bin:$PATH"
export PKG_CONFIG_PATH="$USB_MANAGER_DEPS_PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"

cmake -S usb-manager -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

### Binaries

| Binary | Role |
|--------|------|
| `usb-udev-lab` | P0: print plug/unplug + VID/PID class |
| `usb-manager` | Full daemon (registry, policy, AA demo, CarPlay stub, D-Bus) |
| `usb-manager-selftest` | Offline classifier + exclusive policy checks |

```bash
# P0 lab (needs access to udev; plug a phone)
./build/usb-udev-lab

# Daemon without D-Bus (if session bus missing)
./build/usb-manager --no-dbus --debug
```

## C build (`usb-manager-c/`)

Same behavior, written in C. D-Bus uses **sdbus-c++** from `src/dbus_sdbuspp.cpp` (the only C++ file).

```bash
./scripts/build-c.sh
./build-c/usb-manager-c --no-dbus --debug
```

## D-Bus API

- Service: `org.example.connectivity`
- Path: `/org/example/connectivity/usb`
- Interface: `org.example.connectivity.Usb1`
- Methods: `ListDevices`, `StartSession(device_id, mode)`, `StopSession(device_id)`
- Signals: `DeviceChanged`, `SessionChanged`
- Property: `ActiveSession`

Modes: `android_auto` | `carplay` | `storage`

## Yocto (RPi4)

The Docker environment is Ubuntu 22.04 with the Yocto 5.0.20 (scarthgap) host packages. The first start clones Poky, meta-openembedded, and meta-raspberrypi at the revisions in `docker/layers.lock`, then creates `yocto/build` for `MACHINE = "raspberrypi4-64"`. Source edits under `usb-manager/` are picked up through `externalsrc`.

```bash
./scripts/yocto.sh                 # shell with bitbake on PATH
bitbake core-image-connectivity
```

Non-interactive:

```bash
./scripts/yocto.sh bitbake core-image-connectivity
```

Flash the SD card after `lsblk` confirms the device. `YES` is required on purpose. On WSL, writing the card from Windows (Raspberry Pi Imager, custom image) is the reliable option; `flash-sd` needs a whole-disk node inside the container:

```bash
docker compose run --rm --privileged yocto flash-sd /dev/sdX YES
```

Image:

```
yocto/build/tmp/deploy/images/raspberrypi4-64/core-image-connectivity-raspberrypi4-64.rootfs.wic.bz2
```

First boot: `ssh root@<board-ip>` with an empty password (prototype only). Serial console is GPIO 14/15 at 115200 (`ENABLE_UART`). Use a USB-A host port and a 5V/3A supply.

## Phase mapping

| Phase | What landed |
|-------|-------------|
| P0 | `UdevMonitor`, `DeviceClassifier`, `usb-udev-lab` |
| P1 | Registry, `SessionOrchestrator`, D-Bus service |
| P2 | `TransportProber` (AOAP GET_PROTOCOL + NCM/RNDIS iface scan) |
| P3 | `DemoAasdkSession` + `AndroidAutoAdapter` (see docs/AASDK_INTEGRATION.md) |
| P4 | `CarPlayStubAdapter` (MFI_REQUIRED) + exclusive allowlist policy |
| P5 | `meta-connectivity` recipe, systemd unit, udev rules, image, kernel cfg |

## Học / đọc tài liệu

| Doc | Nội dung |
|-----|----------|
| [docs/READING_ROADMAP.md](docs/READING_ROADMAP.md) | Lộ trình đọc (udev → code → AA/CarPlay → Yocto) |
| [docs/FUNCTIONS.md](docs/FUNCTIONS.md) | Giải thích từng function/API |
| [docs/SEQUENCES.md](docs/SEQUENCES.md) | Sequence diagrams các luồng chính |
| [docs/LIBRARIES.md](docs/LIBRARIES.md) | libudev, libusb, sd-bus, sdbus-c++ (C và C++) |
| [docs/LEARNING.md](docs/LEARNING.md) | Mục lục học + lab nhanh |
| [docs/USB_TESTING.md](docs/USB_TESTING.md) | Test `connectivity-usb`: selftest trên máy dev, cắm thiết bị trên Pi 4 |

## Hardware notes

- Use USB-A host ports on RPi4 (not gadget mode).
- Prefer a 5V/3A+ PSU; phone charging may need a powered hub.
- AOAP control transfers may require `plugdev` / udev permissions on the device node.
