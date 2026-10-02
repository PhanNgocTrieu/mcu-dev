# USB Driver

Thư viện biên với USB Host của Linux, và fragment kernel `kernel/connectivity-usb.cfg`.

Kernel vẫn là usbcore cùng class driver upstream (`cdc_ncm`, `rndis_host`, `usb-storage`, `usbhid`, `cdc-acm`, `ipheth`). Module này đọc kết quả enumeration:

- sysfs: VID/PID, serial, class/subclass/protocol, driver đã bind, netdev, block device
- uevent `add` / `remove` / `change`, biến `PRODUCT`
- usbfs: control transfer cho USB Module (AOA)

Không có driver kernel riêng cho Android Auto, CarPlay, hay AirPlay. Yêu cầu và sequence nằm ở `ASPICE-PLAN.md` và `USB-DRIVER-SOURCE.md`.

## Build

```bash
cmake -S usbDriver -B usbDriver/build
cmake --build usbDriver/build
ctest --test-dir usbDriver/build --output-on-failure
```

Image CM3 kéo fragment qua `meta-cm3-usb` (`linux-raspberrypi`) và gói `usb-driver`.
