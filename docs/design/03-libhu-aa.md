# 3 — Design: libhu-aa (Android Auto media)

## 1. Vai trò

Sở hữu **session media Android Auto** sau khi USB đã ở AOAP / session `active`.

| Làm | Không làm |
|---|---|
| start/stop/poll/touch | AOA control transfer |
| Emit video qua callback | Claim usbfs |
| Hook AASDK khi bật cờ | Session policy (park/idle) |

`usb-managerd` gọi create/start khi `backend=android` + streaming; destroy khi idle/fail.

## 2. Component / API

```mermaid
classDiagram
  class hu_aa_session_t {
    -node
    -cb
    -running
    -tick
    +create(devnode, cb)
    +start()
    +stop()
    +poll()
    +touch(x,y,down)
    +destroy()
  }
  class hu_aa_callbacks_t {
    +on_video(data,len,pts,is_h264,user)
    +on_status(phase,detail,user)
    +user
  }
  hu_aa_session_t --> hu_aa_callbacks_t
  usb_managerd ..> hu_aa_session_t : owns
```

## 3. Hai đường video

```mermaid
flowchart TD
  POLL[hu_aa_poll] --> Q{AASDK hoặc H264_STUB?}
  Q -->|có| H264[emit_h264\npack Annex-B + magic H264\nis_h264=1]
  Q -->|không| RGB[emit_rgb_shim\nmagic FRM1\nis_h264=0]
  H264 --> CB[on_video]
  RGB --> CB
```

| Build | `detail` status | Nguồn AU |
|---|---|---|
| stub ON, no AASDK | `h264-stub` | `hupi_h264_stub_au()` |
| AASDK ON | `aasdk` | TODO messenger → cùng packer |
| stub OFF | `shim` | RGB sọc xanh |

**Điểm thay code AASDK:** `emit_h264()` trong `hu_aa.c` — giữ `hupi_h264_pack_frame()`.

## 4. Sequence với manager

```mermaid
sequenceDiagram
  participant M as managerd
  participant A as libhu-aa

  M->>A: create(device_id, cb)
  M->>A: start()
  A-->>M: on_status(active, …)
  loop poll ~10fps
    M->>A: poll()
    A-->>M: on_video(packet, is_h264)
  end
  M->>A: touch(x,y,down)
  M->>A: destroy()
```

## 5. Dependency

| Hiện có | Cần thêm (thật) |
|---|---|
| `hupi_wire`, `hupi_h264` | `third_party/aasdk` |
| CMake `HUPI_MEDIA_H264_STUB` | Boost/protobuf theo AASDK |
| | Input channel inject trong `hu_aa_touch` |

Spec: [../spec/android-auto-media.md](../spec/android-auto-media.md), [../spec/h264-stream.md](../spec/h264-stream.md).
