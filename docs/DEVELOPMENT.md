# Hướng dẫn phát triển

## Build host (unit test + demo SDL)

```sh
cmake -S . -B build -DHUPI_HOST_SDL=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Tùy chọn hay dùng:

| Cờ | Mặc định | Ý nghĩa |
|---|---|---|
| `HUPI_HOST_SDL` | OFF | Build usb-panel / hupi-cluster |
| `HUPI_MEDIA_H264_STUB` | ON | Emit H.264 stub thay RGB |
| `HUPI_WITH_AASDK` | OFF | Link AASDK (`third_party/aasdk`) |
| `HUPI_WITH_MFI` | OFF | Link MFi/IAP2 |

## Chạy demo lab

```sh
# Terminal 1 — daemons + UI (script repo)
./rasp4/build-demo/scripts/run-host-sim.sh

# Hoặc thủ công (cùng HUPI_RUNTIME)
export HUPI_RUNTIME=/tmp/hupi-demo
mkdir -p "$HUPI_RUNTIME"
./build/modules/usb-driver-userlayer/usb-driverd --runtime "$HUPI_RUNTIME" --log-level trace &
./build/modules/usb-man/usb-managerd --runtime "$HUPI_RUNTIME" --log-level trace &
./build/rasp4/build-demo/apps/usb-panel/usb-demo --runtime "$HUPI_RUNTIME" &
./build/rasp4/build-demo/apps/cluster/hupi-cluster --runtime "$HUPI_RUNTIME"
```

Trên panel: **sim plug android** → session AOA → active → cluster badge `H264` (nếu stub ON).

CLI:

```sh
./build/modules/usb-man/hupi-ctl --runtime "$HUPI_RUNTIME" manager status
./build/modules/usb-man/hupi-ctl --runtime "$HUPI_RUNTIME" manager sim plug android
./build/modules/usb-man/hupi-ctl --runtime "$HUPI_RUNTIME" manager stream on
```

## Log cần xem

```text
aoa.plan id=… steps=9
aa.video h264 stub frame=… bytes=…
media.status phase=active detail=h264-stub
state backend=android phase=active streaming=1 …
```

Board (systemd):

```sh
journalctl -u usb-driverd -u usb-managerd -f
```

## Image Pi 4

```sh
./rasp4/build-raspi4/build-image.sh
```

Cờ media trên Yocto: xem `docs/NEEDED.md` (`EXTRA_OECMAKE:pn-libhu-aa=…`).

## Quy ước code

- Comment logic (tiếng Việt) tại các nhánh khó: AOA, session, queue driver, media.
- Không mở `/dev/bus/usb` từ nhiều process — chỉ `usb-driverd` sau `claim`.
- Một backend projection; máy thứ hai → `parked`.
- Wire text: một dòng một message, `key=value`, escape bằng `hupi_escape`.

## Đọc tiếp

1. [KNOWLEDGE.md](KNOWLEDGE.md) — USB / AOA / CarPlay / SDL / libs  
2. [ARCHITECTURE.md](ARCHITECTURE.md) — sơ đồ process  
3. [MEDIA.md](MEDIA.md) — H.264 / AASDK  
4. [NEEDED.md](NEEDED.md) — thiếu gì cho EVB  

