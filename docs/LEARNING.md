# Learning path — USB Manager / Connectivity SC

Tài liệu học đã được tách thành 3 file chính — **bắt đầu từ đây**:

| File | Nội dung |
|------|----------|
| **[READING_ROADMAP.md](READING_ROADMAP.md)** | Lộ trình đọc tài liệu ngoài + thứ tự đọc code (theo tuần/ngày) |
| **[FUNCTIONS.md](FUNCTIONS.md)** | Giải thích từng class/function trong `usb-manager` |
| **[SEQUENCES.md](SEQUENCES.md)** | Sequence diagrams: boot, AA plug-in, CarPlay stub, unplug, D-Bus, exclusive |
| [AASDK_INTEGRATION.md](AASDK_INTEGRATION.md) | Cách thay `DemoAasdkSession` bằng AASDK/OpenAuto thật |

## Lab nhanh

```bash
./scripts/build.sh
./build/usb-udev-lab              # P0: plug/unplug + classify
./build/usb-manager-selftest      # classifier + exclusive policy
./build/usb-manager --debug       # full daemon
```

## Thứ tự đọc code tối thiểu

`Types.hpp` → `UdevMonitor` → `DeviceClassifier` → `DeviceRegistry` → `ConnectionPolicy` → adapters → `SessionOrchestrator` → `UsbDbusService` → `main.cpp`
