# 0 — System overview & giao tiếp các khối

## 1. Bài toán

Head unit (Pi 4 / EVB) cần:

1. Phát hiện điện thoại USB (Android / iPhone).  
2. Đưa máy vào mode projection đúng (AOA hoặc CDC-NCM).  
3. Chỉ chạy **một** backend media tại một thời điểm.  
4. Đẩy video (+ nhận touch) tới UI / graphics.  
5. Lab chạy được **không cần** điện thoại thật (sim) và **không cần** AASDK/MFi ngay (stub).

## 2. Context diagram

```mermaid
flowchart LR
  Phone["Điện thoại\nAndroid / iPhone"]
  Kernel["Linux USB host\nsysfs · uevent · usbfs · cdc_ncm"]
  Driver["usb-driverd"]
  Man["usb-managerd"]
  AA["libhu-aa"]
  CP["libhu-carplay"]
  UI["Demo UI / Graphics\npanel · cluster"]

  Phone <--> Kernel
  Kernel <--> Driver
  Driver <-->|usb-driver.sock| Man
  Man --> AA
  Man --> CP
  AA -.->|tương lai AASDK| Phone
  CP -.->|tương lai IAP2/MFi| Phone
  Man <-->|usb-manager.sock| UI
  Man -->|usb-stream.sock| UI
```

## 3. Component diagram (trong repo)

```mermaid
flowchart TB
  subgraph apps["Apps / daemons"]
    DRV["usb-driverd"]
    MAN["usb-managerd"]
    CTL["hupi-ctl"]
    PANEL["usb-demo panel"]
    CLUS["hupi-cluster"]
  end

  subgraph libs["Libraries"]
    USBDRV["libusbdrv\nenum · uevent · usbfs"]
    USBMAN["libusbman\nclassify · session · aoa"]
    HUAA["libhu-aa"]
    HUCP["libhu-carplay"]
    WIRE["libhupi_wire\nunix · kv · log · h264 stub"]
  end

  subgraph kernel["Kernel / OS"]
    SYSFS["sysfs USB"]
    NL["netlink uevent"]
    USBFS["/dev/bus/usb"]
    NCM["cdc_ncm + net iface"]
  end

  DRV --> USBDRV --> SYSFS
  USBDRV --> NL
  USBDRV --> USBFS
  DRV --> WIRE
  MAN --> USBMAN
  MAN --> HUAA
  MAN --> HUCP
  MAN --> WIRE
  HUAA --> WIRE
  HUCP --> WIRE
  MAN -.-> NCM
  CTL --> WIRE
  PANEL --> WIRE
  CLUS --> WIRE
```

### Vì sao tách khối như vậy?

| Khối | Trách nhiệm duy nhất | Vì sao không gộp |
|---|---|---|
| `usb-driverd` | Biên giới USB host (enum, claim, ctrl) | Nhiều client có thể quan sát device; chỉ **một** chỗ mở usbfs |
| `usbman` (lib) | Policy thuần — test được không cần USB | Tách logic khỏi I/O / poll loop |
| `usb-managerd` | Orchestration + media + stream | Ghép policy với AOA queue và lib media |
| `libhu-aa` / `libhu-carplay` | Protocol media riêng từng OS | AASDK ≠ MFi; thay độc lập |
| `hupi_wire` | Transport lab chung | Tránh mỗi daemon tự invent socket framing |

## 4. Deployment (lab vs board)

```mermaid
flowchart LR
  subgraph lab["Host lab"]
    D1[usb-driverd]
    M1[usb-managerd]
    P1[usb-demo]
    C1[hupi-cluster]
    D1 --- M1 --- P1
    M1 --- C1
  end

  subgraph board["Pi 4 / EVB image"]
    D2[usb-driverd.service]
    M2[usb-managerd.service]
    G2[hu-graphics / Weston]
    D2 --- M2 --- G2
  end
```

| Môi trường | Runtime dir | Video consumer |
|---|---|---|
| Lab | `$HUPI_RUNTIME` /tmp | SDL cluster |
| Board | `/run/hupi` | graphics daemon (sau này) |

## 5. Sequence — Android Auto (happy path)

```mermaid
sequenceDiagram
  autonumber
  participant Phone
  participant Kernel
  participant Driver as usb-driverd
  participant Man as usb-managerd
  participant AA as libhu-aa
  participant UI as cluster/panel

  Phone->>Kernel: plug ADB
  Kernel->>Driver: uevent add
  Driver->>Driver: refresh sysfs
  Driver-->>Man: dev id=… adb=1
  Man->>Man: classify ANDROID → phase=aoa
  Man->>Driver: claim + GET_PROTOCOL + SEND_STRING×6 + START
  Driver->>Phone: usbfs control (AOA)
  Phone->>Kernel: re-enum AOAP
  Driver-->>Man: dev gone (pending_reenum)
  Driver-->>Man: dev … accessory=1
  Man->>Man: phase=active backend=android
  Man->>AA: create/start
  AA-->>Man: on_video(H264 or RGB)
  Man-->>UI: state … streaming=1
  Man-->>UI: usb-stream frame
  UI->>Man: touch x y down
  Man->>AA: hu_aa_touch
```

## 6. Sequence — CarPlay (happy path)

```mermaid
sequenceDiagram
  autonumber
  participant Phone
  participant Kernel
  participant Driver as usb-driverd
  participant Man as usb-managerd
  participant CP as libhu-carplay
  participant UI

  Phone->>Kernel: plug + CDC-NCM
  Kernel->>Driver: uevent
  Driver-->>Man: dev … apple=1 ncm=1 net=usb0
  Man->>Man: classify CARPLAY → active
  Man->>Kernel: ip link set usb0 up
  Man->>CP: create(iface)/start
  Note over CP: MFi/IAP2 thật sau này;\nhiện H.264 stub
  CP-->>Man: on_video
  Man-->>UI: state + stream frames
```

## 7. Sequence — hai máy cùng lúc (parking)

```mermaid
sequenceDiagram
  participant Man as usb-managerd
  participant D as usb-driverd

  D-->>Man: dev sim-android → active android
  D-->>Man: dev sim-carplay
  Man->>Man: parked=sim-carplay reason=backend-busy
  Note over Man: Không preempt session đang chạy
  D-->>Man: dev gone sim-android
  Man->>Man: idle → promote parked/known
  Man->>Man: active carplay
```

## 8. Bản đồ file → khối

| Khối | Sources chính |
|---|---|
| usb-driver | `modules/usb-driver-userlayer/` |
| usb-manager | `modules/usb-man/` |
| AA media | `modules/libhu-aa/` |
| CarPlay media | `modules/libhu-carplay/` |
| Common | `modules/common/` |
| Demo UI | `rasp4/build-demo/apps/` |

Tiếp theo: chi tiết từng khối trong các file `01`…`06`.
