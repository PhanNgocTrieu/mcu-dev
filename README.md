# HUPI module-usb

Các module USB tách riêng (không còn blob “connectivity” đơn), dùng chung cho EVB và lab Raspberry Pi 4.

## Tài liệu

Bắt đầu từ **[docs/README.md](docs/README.md)** (mục lục đầy đủ).

| Nhóm | File | Nội dung |
|---|---|---|
| **Design + diagram** | [docs/design/](docs/design/README.md) | Giao tiếp khối, component, sequence, state machine |
| **Specification** | [docs/spec/](docs/spec/README.md) | AOA/CarPlay/wire/H.264 — *cần gì & vì sao* |
| Kiến thức nền | [docs/KNOWLEDGE.md](docs/KNOWLEDGE.md) | USB, AOA, SDL, danh sách lib |
| Build / demo | [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | cmake, sim, log |
| Media checklist | [docs/MEDIA.md](docs/MEDIA.md) | Stub → AASDK/MFi |
| Board gaps | [docs/NEEDED.md](docs/NEEDED.md) | Input còn thiếu |

Gợi ý đọc lần đầu: [docs/design/00-system-overview.md](docs/design/00-system-overview.md) → [docs/spec/aoa.md](docs/spec/aoa.md) → [docs/spec/carplay-ncm.md](docs/spec/carplay-ncm.md).

## Cấu trúc code

```text
modules/
  usb-driver-userlayer/   # plug/unplug, enumerate, usbfs lease
  usb-man/                # session policy, AOA, one backend
  libhu-aa/               # Android Auto media (AASDK-ready + H.264 stub)
  libhu-carplay/          # CarPlay media over NCM (MFi-ready + H.264 stub)
  common/                 # wire protocol + logger + H.264 packer
meta-connectivity/        # Yocto layer
meter-pf/                 # EVB configs
rasp4/
  build-raspi4/           # image flash được
  build-demo/             # monitor ảo (SDL)
tools/
docs/
```

## Media (tóm tắt)

Mặc định lab emit **H.264 stub** (magic `H264`) để nối pipeline trước AASDK/MFi.  
Tắt stub: `-DHUPI_MEDIA_H264_STUB=OFF`. Chi tiết: [docs/MEDIA.md](docs/MEDIA.md).

| Backend | Library | Cờ board |
|---|---|---|
| Android Auto | `libhu-aa` | `-DHUPI_WITH_AASDK=ON` + `third_party/aasdk` |
| CarPlay | `libhu-carplay` | `-DHUPI_WITH_MFI=ON` + libiap2/libhu-mfi |

## Build nhanh

```sh
cmake -S . -B build -DHUPI_HOST_SDL=ON && cmake --build build && ctest --test-dir build
./rasp4/build-demo/scripts/run-host-sim.sh
```
