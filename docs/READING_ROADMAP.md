# Lộ trình đọc tài liệu — USB Manager / Connectivity SC

Tài liệu này dẫn bạn đọc **theo thứ tự**: nền tảng USB → code trong repo → AA/CarPlay → Yocto.  
Song song xem [FUNCTIONS.md](FUNCTIONS.md) (giải thích API) và [SEQUENCES.md](SEQUENCES.md) (sequence diagrams).

---

## Cách dùng lộ trình

| Ngày (gợi ý) | Mục tiêu | Lab trên máy |
|--------------|----------|--------------|
| Ngày 1–2 | Hiểu USB Host + udev | `udevadm monitor`, `usb-udev-lab` |
| Ngày 3–4 | Hiểu orchestrator + policy | Đọc code + `usb-manager-selftest` |
| Ngày 5–6 | AOAP/NCM + AA demo path | Cắm Android, xem log probe |
| Ngày 7 | CarPlay stub + D-Bus | `busctl` / stub `MFI_REQUIRED` |
| Ngày 8+ | Yocto đóng gói | Đọc `meta-connectivity` |

Mỗi mục bên dưới: **đọc gì → vì sao → map sang file nào trong repo**.

---

## Tuần 1 — Nền tảng (đọc trước khi đào sâu code)

### Bước 1. USB Host vs Gadget (30–60 phút)

**Đọc**
- Kernel USB overview (Host controller, enumeration, endpoints)
- Phân biệt: RPi4 **USB-A = Host** (phone là Device) — đúng design của dự án

**Hiểu được gì**
- Ai là Host, ai gửi SETUP/control transfer
- Vì sao CarPlay/AA wired trên HU thường là Host mode

**Map repo:** toàn bộ `usb-manager` giả định Host; không dùng dwc2 gadget trên USB-A.

### Bước 2. udev / libudev (1–2 giờ)

**Đọc**
- [libudev man](https://www.freedesktop.org/software/systemd/man/libudev.html) — `udev_monitor`, `udev_enumerate`, properties `ID_VENDOR_ID`, `ID_MODEL_ID`, `ID_USB_INTERFACES`
- Chạy: `udevadm monitor --subsystem-match=usb --property`

**Lab**
```bash
./scripts/build.sh
./build/usb-udev-lab
# cắm/rút USB → xem ADD/REMOVE + type=
```

**Map repo**
- [`UdevMonitor`](../usb-manager/src/hotplug/UdevMonitor.cpp) — `start`, `pollOnce`, `enumerateExisting`
- [`DeviceClassifier`](../usb-manager/src/classify/DeviceClassifier.cpp) — `classify`, `enrich`

### Bước 3. libusb cơ bản (1 giờ)

**Đọc**
- [libusb API](https://libusb.info/) — `libusb_open_device_with_vid_pid`, `libusb_control_transfer`

**Map repo**
- [`TransportProber::probeAoap`](../usb-manager/src/aoap/TransportProber.cpp) — AOAP `GET_PROTOCOL` (request 51)

### Bước 4. D-Bus / sd-bus (1 giờ)

**Đọc**
- systemd `sd-bus` — service name, object path, interface, method/signal
- Sketch trong design: `org.example.connectivity.Usb1`

**Map repo**
- [`UsbDbusService`](../usb-manager/src/ipc/UsbDbusService.cpp)

---

## Tuần 2 — Đọc code trong repo (theo dependency)

Đọc **header trước, .cpp sau**. Thứ tự bắt buộc:

```text
1. Types.hpp
2. Log.hpp / Log.cpp
3. DeviceClassifier
4. UdevMonitor
5. DeviceRegistry
6. ConnectionPolicy
7. IProjectionAdapter + TransportProber
8. AndroidAutoAdapter + DemoAasdkSession + CarPlayStubAdapter
9. SessionOrchestrator
10. UsbDbusService
11. main.cpp
```

Chi tiết từng function: [FUNCTIONS.md](FUNCTIONS.md)  
Luồng runtime: [SEQUENCES.md](SEQUENCES.md)

### Checklist hiểu code

- [ ] Phân biệt `DeviceState` vs `SessionState` / `SessionMode`
- [ ] Hotplug Add → `onDeviceAdded` → `probeDevice` → (AA) `startSession`
- [ ] Policy `exclusive_session` từ chối CarPlay khi AA đang Active
- [ ] CarPlay luôn fail với `MFI_REQUIRED` (đúng prototype A)
- [ ] HMI chỉ nói chuyện qua D-Bus, không gọi libudev

---

## Tuần 3 — Android Auto & CarPlay (học concept)

### Android Auto (open-source)

**Đọc theo thứ tự**
1. Android Open Accessory (AOAP) — GET_PROTOCOL / START / accessory PID `2d00–2d05`
2. [AASDK](https://github.com/f1xpl/aasdk) — README + cấu trúc channel (video/audio/input)
3. [OpenAuto](https://github.com/f1xpl/openauto) — cách HU gắn transport + UI (chỉ tham chiếu kiến trúc)
4. Repo: [docs/AASDK_INTEGRATION.md](AASDK_INTEGRATION.md) — cách thay `DemoAasdkSession`

**Lab:** chạy `usb-manager --debug`, cắm Android, đọc log `AOAP:` / `NCM` / `AASDK demo:`

### CarPlay (chỉ concept — không OSS đầy đủ)

**Đọc**
- Public overview: iAP2, USB NCM, MFi auth chip (production)
- Trong code: `CarPlayStubAdapter` — `Available=false`, `Reason=MFI_REQUIRED`

**Không** cố clone full CarPlay stack trong prototype học.

---

## Tuần 4 — Connectivity SC & Yocto

### Connectivity SC (những phần “hay quên”)

Đọc lại design §2.6 — các sibling sau này:
- `bt-manager` / Wi‑Fi SoftAP (wireless AA/CP)
- Mass storage mount policy
- Allowlist / last-connected (đã có skeleton trong `ConnectionPolicy`)
- Health: reconnect, hub reset

### Yocto

**Đọc**
- Yocto Mega Manual — layers, `IMAGE_INSTALL`, systemd class
- meta-raspberrypi — `MACHINE=raspberrypi4-64`

**Map repo**
- [`meta-connectivity/`](../meta-connectivity/) — `usb-manager_0.1.0.bb`, `core-image-connectivity.bb`, `connectivity-usb.cfg`
- Sample: [`sample-local.conf.snippet`](../meta-connectivity/conf/sample-local.conf.snippet)

---

## Thứ tự ưu tiên nếu ít thời gian (1 ngày)

1. `Types.hpp` + sequence “Android plug-in” trong [SEQUENCES.md](SEQUENCES.md)
2. `UdevMonitor` + `DeviceClassifier` + chạy `usb-udev-lab`
3. `SessionOrchestrator::onDeviceAdded` / `startSession` / `stopSession`
4. `ConnectionPolicy::canStart` + selftest
5. `AndroidAutoAdapter::probe` + `DemoAasdkSession::start`
6. D-Bus methods trong `UsbDbusService.cpp` (skim)

---

## Tài liệu ngoài (bookmark)

| Chủ đề | Link / nguồn |
|--------|----------------|
| libudev | https://www.freedesktop.org/software/systemd/man/libudev.html |
| libusb | https://libusb.info/ |
| AASDK | https://github.com/f1xpl/aasdk |
| OpenAuto | https://github.com/f1xpl/openauto |
| AOAP | Android Open Accessory protocol docs |
| Yocto | https://docs.yoctoproject.org/ |
| meta-raspberrypi | https://github.com/agherzan/meta-raspberrypi |

---

## Sau khi đọc xong bạn nên giải thích được

1. Phone cắm vào → sự kiện nào → class nào xử lý?
2. Vì sao `usb-manager` không decode video?
3. AOAP và NCM khác nhau thế nào trong probe?
4. Exclusive session ngăn case nào?
5. Muốn gắn AASDK thật thì sửa class nào?
