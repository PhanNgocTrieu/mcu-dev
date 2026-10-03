# 4 — Design: libhu-carplay (CarPlay media)

## 1. Vai trò

Session media CarPlay **sau** khi có CDC-NCM (+ link up). Không làm AOA; không classify.

| Làm | Không làm |
|---|---|
| create(iface, endpoint) / start / poll / touch | `ip link` (manager làm) |
| Hook IAP2/MFi khi có | Enum USB |
| Emit video callback | Auth cert (SDK bên ngoài) |

## 2. Component / API

```mermaid
classDiagram
  class hu_carplay_session_t {
    -iface
    -endpoint
    -cb
    -running
    -media_fd
    +create(iface, endpoint, cb)
    +start()
    +stop()
    +poll()
    +touch()
    +destroy()
  }
  class hu_carplay_callbacks_t {
    +on_video(...)
    +on_status(...)
  }
  hu_carplay_session_t --> hu_carplay_callbacks_t
```

Default endpoint lab: `10.10.10.1:5000` trên iface `usb0` (hoặc tên từ session.net).

## 3. Đường media

```mermaid
flowchart TD
  START[hu_carplay_start] --> MFI{HUPI_WITH_MFI?}
  MFI -->|ON| TODO[TODO: IAP2 auth + TCP media]
  MFI -->|OFF| STUB[H264 stub hoặc RGB]
  POLL[poll] --> OUT[on_video]
  TODO --> POLL
  STUB --> POLL
```

| Cờ | Status detail | Video |
|---|---|---|
| MFI ON | `mfi` | TODO TCP AU |
| stub ON | `h264-stub` | Annex-B hardcode |
| cả hai OFF | `ncm-shim` | RGB xanh dương |

## 4. Quan hệ với NCM / MFi

```mermaid
flowchart LR
  USB[iPhone USB] --> NCM[cdc_ncm iface]
  NCM --> IP[IPv6/IPv4 link]
  IP --> IAP[IAP2 + MFi]
  IAP --> MED[media TCP H.264]
  MED --> LIB[libhu-carplay]
```

**Vì sao không ipheth?** Policy HUPI / CarPlay wired hiện đại yêu cầu NCM; ipheth là tether cũ, không đủ đường CarPlay chuẩn trong thiết kế này.

## 5. File & dependency

| File | Vai trò |
|---|---|
| `hu_carplay.c` | Session + stub/poll |
| `hu_carplay.h` | API |

Cần thêm: `libiap2`, `libhu-mfi`, cert MFi — xem [../NEEDED.md](../NEEDED.md).  
Spec: [../spec/carplay-ncm.md](../spec/carplay-ncm.md).
