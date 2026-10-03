# 2 — Design: khối usb-manager

## 1. Vai trò

**Orchestrator** projection: nhận device events → policy session → AOA / link-up → bật media → stream video + nhận touch/UI.

| Làm | Không làm |
|---|---|
| Classify + state machine | Mở usbfs trực tiếp |
| Queue lệnh tuần tự xuống driver | Implement AASDK/MFi |
| `media_sync` AA/CarPlay | Decode H.264 |
| Publish `state` + binary stream | Enum sysfs |

Tách **lib `usbman`** (pure) khỏi **daemon `managerd`** (I/O) để unit-test policy không cần USB.

## 2. Component nội bộ

```mermaid
flowchart TB
  subgraph daemon["usb-managerd"]
    LOOP["poll loop"]
    Q["command queue → driver"]
    PUB["publish state"]
    MED["media_sync / consider_frame"]
    STR["stream accept/write"]
  end

  subgraph lib["libusbman"]
    CL["classify.c"]
    SE["session.c"]
    AO["aoa.c"]
    PAR["parse_dev"]
  end

  subgraph media["media libs"]
    AA["libhu-aa"]
    CP["libhu-carplay"]
  end

  LOOP --> Q
  LOOP --> MED
  LOOP --> STR
  Q --> CL
  LOOP --> SE
  SE --> CL
  AO --> Q
  MED --> AA
  MED --> CP
  PUB --> SE
```

### “Class” / struct

```text
usbman_dev_t       — device đã parse từ dòng "dev …"
usbman_session_t   — backend/phase/streaming/parked/…
usbman_kind_t      — kết quả classify
usbman_action_t    — NONE | AOA | LINK_UP
qitem              — 1 lệnh chờ gửi driver (+ cờ aoa)
```

## 3. Giao tiếp

```mermaid
flowchart LR
  D[usb-driverd] <-->|driver.sock| M[usb-managerd]
  UI[panel/ctl/cluster] <-->|manager.sock| M
  M -->|stream.sock| CL[cluster/graphics]
  M --> AA[libhu-aa]
  M --> CP[libhu-carplay]
  M -->|fork ip| IP[ip link set]
```

### API manager.sock

| Lệnh | Ý nghĩa |
|---|---|
| `sub` / `status` | Nhận / hỏi `state …` |
| `stream on\|off` | User override streaming |
| `touch x y down` | 0…10000 → session + media |
| `sim …` | Proxy nguyên dòng xuống driver queue |

## 4. State machine session

```mermaid
stateDiagram-v2
  [*] --> idle
  idle --> aoa: ANDROID detected
  aoa --> reenumerating: START sent + gone
  reenumerating --> active: AOAP appears
  aoa --> failed: ctrl/protocol error
  idle --> classified: Apple wait NCM
  classified --> active: NCM ready
  idle --> active: CarPlay NCM / AOAP direct
  idle --> failed: Apple ipheth
  active --> idle: device gone (no pending_reenum)
  failed --> idle: device gone / new adopt
  active --> active: touch/stream updates
```

| Phase | Ý nghĩa UX / media |
|---|---|
| `idle` | Không projection |
| `aoa` | Đang chạy chuỗi AOA |
| `reenumerating` | Chờ phone plug lại AOAP |
| `classified` | Apple thấy, chờ NCM |
| `active` | Có thể stream |
| `failed` | Lỗi / policy từ chối |

## 5. Classify → action

```mermaid
flowchart TD
  D[usbman_dev_t] --> K{usbman_classify}
  K -->|ANDROID| A1[ACT_AOA]
  K -->|ANDROID_AOAP| A2[active, ACT_NONE]
  K -->|CARPLAY| A3[active + ACT_LINK_UP*]
  K -->|APPLE_WAIT| A4[classified]
  K -->|APPLE_IPHETH| A5[failed]
  K -->|other| A6[other=id, NONE]
```

\* sim CarPlay: không `ip link` (không có iface thật).

## 6. Queue AOA / pump

```mermaid
sequenceDiagram
  participant S as session
  participant Q as queue
  participant D as driver

  S->>Q: enqueue claim..START (aoa=1)
  loop until queue empty
    Note over Q: nếu head là START → expect_reenum
    Q->>D: send one line
    D-->>Q: ok / err
    alt err on aoa
      Q->>S: fail + drop remaining aoa
    else ok GET_PROTOCOL
      Q->>Q: check protocol ≥ 1
    end
  end
```

**Vì sao tuần tự?** Mỗi bước AOA phụ thuộc bước trước; parallel ctrl làm hỏng START/re-enum.

## 7. Media & stream

```mermaid
flowchart TD
  P{phase=active AND streaming?} -->|không| STOP[media_stop]
  P -->|android| AA[hu_aa_create/start]
  P -->|carplay| CP[hu_carplay_create/start]
  AA -->|on_video| BUF[g_media_frame]
  CP -->|on_video| BUF
  BUF --> STREAM[usb-stream.sock]
  BUF -.->|không có media| RGB[fill_frame RGB shim trong manager]
```

Một consumer stream tại một thời điểm (client mới thay client cũ).

## 8. File map

| File | Trách nhiệm |
|---|---|
| `managerd.c` | Daemon: poll, queue, media, stream |
| `session.c` | State machine |
| `classify.c` | kind + parse_dev |
| `aoa.c` | Build chuỗi lệnh AOA |
| `ctl.c` | CLI `hupi-ctl` |

Spec: [../spec/aoa.md](../spec/aoa.md), [../spec/carplay-ncm.md](../spec/carplay-ncm.md), [../spec/wire-protocol.md](../spec/wire-protocol.md).
