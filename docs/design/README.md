# Design documentation — mục lục

Tài liệu **thiết kế chi tiết** theo từng khối: sơ đồ giao tiếp, component, sequence, state machine, và *vì sao* tách như vậy.

## Đọc theo thứ tự

| # | File | Nội dung |
|---|---|---|
| 0 | [00-system-overview.md](00-system-overview.md) | Context, component diagram, luồng AA/CarPlay end-to-end |
| 1 | [01-usb-driver.md](01-usb-driver.md) | Khối `usb-driverd` + lib `usbdrv` |
| 2 | [02-usb-manager.md](02-usb-manager.md) | Khối `usb-managerd` + lib `usbman` |
| 3 | [03-libhu-aa.md](03-libhu-aa.md) | Media Android Auto |
| 4 | [04-libhu-carplay.md](04-libhu-carplay.md) | Media CarPlay / NCM |
| 5 | [05-common-wire.md](05-common-wire.md) | Wire protocol, log, H.264 packer |
| 6 | [06-demo-ui.md](06-demo-ui.md) | SDL panel + cluster |

## Specification (chuẩn / yêu cầu / vì sao cần)

| File | Chủ đề |
|---|---|
| [../spec/README.md](../spec/README.md) | Mục lục spec |
| [../spec/aoa.md](../spec/aoa.md) | Android Open Accessory |
| [../spec/android-auto-media.md](../spec/android-auto-media.md) | AA media / AASDK |
| [../spec/carplay-ncm.md](../spec/carplay-ncm.md) | CarPlay wired + NCM + MFi |
| [../spec/wire-protocol.md](../spec/wire-protocol.md) | Unix-line + stream binary |
| [../spec/h264-stream.md](../spec/h264-stream.md) | Frame video FRM1 / H264 |

## Tài liệu nền

- [../KNOWLEDGE.md](../KNOWLEDGE.md) — kiến thức nền USB/AOA/SDL/libs  
- [../ARCHITECTURE.md](../ARCHITECTURE.md) — tóm tắt kiến trúc  
- [../MEDIA.md](../MEDIA.md) — checklist nối media thật  
- [../NEEDED.md](../NEEDED.md) — thiếu gì trên board  
