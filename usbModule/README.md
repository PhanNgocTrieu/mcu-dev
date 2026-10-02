# USB Module

Tiến trình userspace phía trên USB Driver. Nó giữ enumeration, phân loại thiết bị, phiên AOA, và phiên AirPlay ở mức transport.

| Việc | Module này | Không nằm ở đây |
|---|---|---|
| Enumeration | đọc sysfs/uevent qua `usbdrv` | reset cổng, cấp địa chỉ (usbcore) |
| AOA | GET_PROTOCOL (51), SEND_STRING (52), START (53) trên usbfs | video/audio Android Auto |
| AirPlay | máy Apple (VID `05ac`) đã có NCM, ECM, RNDIS, hoặc `ipheth` thì phiên `airplay` sang active | RAOP, FairPlay, nội dung media |
| Mass storage / HID | phân loại và trạng thái ready | mount, vẽ UI |

Hai phiên projection (AOA và AirPlay) không chạy cùng lúc.

## Build

```bash
cmake -S usbModule -B usbModule/build -DUSBDRV_SOURCE_DIR="$PWD/usbDriver"
cmake --build usbModule/build
ctest --test-dir usbModule/build --output-on-failure
```

## Chạy

```bash
./usbModule/build/usb-module --once
./usbModule/build/usb-module --no-aoa
```

`--once` in các thiết bị đang cắm rồi thoát. Không có cờ này thì tiến trình theo dõi uevent.

Cờ mặc định trên dòng lệnh là gửi AOA khi điện thoại Android vừa enumerate. `--no-aoa` chỉ ghi nhận máy và để probe ở trạng thái deferred. Unit systemd cài `--no-aoa` để image lab không đổi mode điện thoại lúc boot. Bỏ cờ đó khi cổng phone phải chuyển accessory.

AirPlay chỉ active khi đã có interface mạng. Điện thoại Apple chưa expose NCM/`ipheth` vẫn enumerate, phiên AirPlay trả lỗi thiếu transport.
