# Kiến trúc HUPI module-usb

Tài liệu mô tả các process, socket và luồng dữ liệu trong repo `mcu-dev`.

## Tổng quan

```text
┌─────────────┐   uevent/sysfs    ┌──────────────────┐
│  USB phone  │ ───────────────► │   usb-driverd     │
└─────────────┘   usbfs ctrl     │  (userlayer)      │
                                 └────────┬─────────┘
                                          │ unix: usb-driver.sock
                                          │  dev … / ok|err
                                          ▼
┌─────────────┐   state/touch     ┌──────────────────┐
│ usb-panel / │ ◄───────────────► │   usb-managerd   │
│ hupi-ctl    │   manager.sock    │  (session+media) │
└─────────────┘                   └────────┬─────────┘
                                           │ libhu-aa / libhu-carplay
                                           │ usb-stream.sock (video)
                                           ▼
                                    ┌──────────────┐
                                    │ hupi-cluster │
                                    │ (demo SDL)   │
                                    └──────────────┘
```

Chỉ **một** backend projection tại một thời điểm: Android Auto **hoặc** CarPlay.

## Thư mục

| Path | Vai trò |
|---|---|
| `modules/common/` | Wire protocol, log, H.264 stub packer |
| `modules/usb-driver-userlayer/` | Daemon `usb-driverd`: enum, claim, usbfs, sim |
| `modules/usb-man/` | Daemon `usb-managerd` + lib `usbman` (policy) + `hupi-ctl` |
| `modules/libhu-aa/` | Session media Android Auto |
| `modules/libhu-carplay/` | Session media CarPlay (NCM) |
| `rasp4/build-demo/` | Demo host SDL (panel + cluster) |
| `rasp4/build-raspi4/` | Image Yocto Pi 4 |
| `meter-pf/` | Cấu hình EVB |
| `meta-connectivity/` | Recipe Yocto |
| `docs/` | Tài liệu (file này + MEDIA, DEVELOPMENT, NEEDED) |

## Process & socket

Runtime mặc định: `/run/hupi` hoặc `$HUPI_RUNTIME` / `--runtime DIR`.

| Socket | Server | Client | Nội dung |
|---|---|---|---|
| `usb-driver.sock` | usb-driverd | usb-managerd, hupi-ctl | `sub`, `claim`, `ctrl`, `sim …`, sự kiện `dev …` |
| `usb-manager.sock` | usb-managerd | panel, cluster, hupi-ctl | `sub`, `status`, `stream`, `touch`, `sim …` |
| `usb-stream.sock` | usb-managerd | cluster / graphics | Binary frame FRM1 hoặc H264 |

## Session policy (usbman)

Phase Android: `idle → aoa → reenumerating → active`  
Phase Apple: `idle → classified → active` (cần CDC-NCM; `ipheth` → `failed`)

Chi tiết state machine: `modules/usb-man/src/session.c`.

## Media

Xem [MEDIA.md](MEDIA.md) — RGB shim, H.264 stub, bước nối AASDK/MFi.

## Tài liệu liên quan

- **[design/](design/README.md)** — detailed design + Mermaid diagrams từng khối  
- **[spec/](spec/README.md)** — AOA / CarPlay / wire / H.264 (cần gì & vì sao)  
- [KNOWLEDGE.md](KNOWLEDGE.md) — USB, AOA, CarPlay, SDL, dependency  
- [DEVELOPMENT.md](DEVELOPMENT.md) — build, chạy demo, test  
- [NEEDED.md](NEEDED.md) — thiếu gì để lên board production  
- [MEDIA.md](MEDIA.md) — video / H.264
