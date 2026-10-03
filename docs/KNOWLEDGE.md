# Kiến thức trọng tâm cho project HUPI module-usb

Tài liệu này giải thích **khái niệm cần nắm** khi đọc/sửa code trong repo — gắn với chỗ dùng thực tế, không phải encyclopedia USB đầy đủ.

Đọc kèm:

- **Design + diagram:** [design/README.md](design/README.md)  
- **Spec (cần gì / vì sao):** [spec/README.md](spec/README.md)  
- [ARCHITECTURE.md](ARCHITECTURE.md) · [MEDIA.md](MEDIA.md) · [DEVELOPMENT.md](DEVELOPMENT.md) · [NEEDED.md](NEEDED.md)

---

## 1. USB host trên Linux (user-space)

### Host vs device vs gadget

| Vai trò | Ai đóng | Trong project |
|---|---|---|
| **USB host** | Board (Pi 4 / EVB) | `usb-driverd` đọc kernel host stack |
| **USB device** | Điện thoại | Android / iPhone cắm vào |
| **USB gadget** | Board đóng vai device | **Không** dùng trong lab hiện tại |

Kernel đã enumerate thiết bị; user-space **không thay usbcore**, chỉ quan sát và gửi control transfer khi cần.

### sysfs

Đường điển hình: `/sys/bus/usb/devices/<bus>-<port>/`

| File / thư mục | Ý nghĩa | Code |
|---|---|---|
| `idVendor` / `idProduct` | VID/PID hex | `enum.c` |
| `manufacturer`, `product`, `serial` | Chuỗi mô tả | `enum.c` |
| `busnum`, `devnum` | → `/dev/bus/usb/BBB/DDD` | `enum.c` |
| `*:*.*` (có dấu `:`) | Interface: class/subclass/protocol + driver | `note_iface()` |
| `…/net/` | Tên iface mạng (vd. `usb0`) nếu CDC | `find_net()` |

Class quan trọng với project:

| Class / driver | Cờ nội bộ | Dùng cho |
|---|---|---|
| `ff/42/01` hoặc ADB | `adb` | Android trước AOA |
| Google AOAP pid `0x2d00`/`0x2d01` | `accessory` | Android Auto sau AOA |
| CDC-NCM `02/0d` / `cdc_ncm` | `ncm` | CarPlay |
| `ipheth` | `ipheth` | **Bị policy từ chối** |
| Mass storage / HID | `storage` / `hid` | Không projection |

### uevent (netlink)

Kernel multicast sự kiện `add` / `remove` / `change` trên subsystem `usb`.  
`usb-driverd` lắng nghe → **quét lại sysfs** (không tin uevent là nguồn state duy nhất).

### usbfs + control transfer

Node: `/dev/bus/usb/<bus>/<dev>`.  
IOCTL `USBDEVFS_CONTROL` gửi USB setup packet:

| Field | Ý nghĩa |
|---|---|
| `bmRequestType` | Hướng + type (bit7: IN=1 / OUT=0) |
| `bRequest` | Mã lệnh (AOA: 51/52/53) |
| `wValue`, `wIndex`, `wLength` | Tham số + độ dài data |

**Claim exclusive** trong `usb-driverd`: chỉ một peer được `claim` trước khi `ctrl` — tránh hai process AOA cùng lúc.

### VID/PID cần nhớ

| VID | Ai | Ghi chú |
|---|---|---|
| `0x18d1` | Google | Android / AOAP |
| `0x05ac` | Apple | iPhone |
| `0x1d6b` | Linux root hub | Bỏ qua khi enum |

---

## 2. Android Open Accessory (AOA)

AOA cho phép **host (head unit)** yêu cầu phone Android vào **accessory mode** để chạy giao thức kiểu Android Auto trên bulk USB.

### Chuỗi chuẩn (đúng thứ tự trong `aoa.c`)

1. **claim** thiết bị  
2. **GET_PROTOCOL** — `bm=0xC0`, `bRequest=51`, IN 2 byte → protocol ≥ 1  
3. **SEND_STRING × 6** — `bm=0x40`, `bRequest=52`, `wIndex = 0…5`  
4. **START** — `bm=0x40`, `bRequest=53` → phone **re-enumerate** (unplug tạm rồi plug lại AOAP)

Sáu chuỗi SEND_STRING:

| Index | Ý nghĩa | Ví dụ trong repo |
|---|---|---|
| 0 | manufacturer | `HUPI` |
| 1 | model | `Raspberry Pi 4` |
| 2 | description | `Android Auto` |
| 3 | version | `1.0` |
| 4 | URI | `https://hupi.local/aa` |
| 5 | serial | `HUPI-RPI4` |

### Re-enumeration

Sau START, phone biến mất rồi xuất hiện với PID AOAP (`0x2d00` / `0x2d01`).  
Session đặt `pending_reenum` → phase `reenumerating` thay vì về `idle` ngay khi thấy `dev gone`.

### AOA ≠ Android Auto media

- **AOA**: chuyển mode USB (control transfer).  
- **Android Auto media**: session AASDK / protobuf / video H.264 / input — nằm ở `libhu-aa` (hiện stub H.264).

---

## 3. Android Auto (media)

| Thành phần | Vai trò trong repo |
|---|---|
| Phone ở AOAP | Endpoint bulk / accessory |
| `libhu-aa` | Session media: start/stop, touch, `on_video` |
| **AASDK** (f1xpl/aasdk) | Stack open-source thật — `HUPI_WITH_AASDK=ON` |
| H.264 stub | Thay messenger khi chưa có AASDK |

Luồng mong muốn sau này:

```text
AOAP device → AASDK messenger → H.264 AU → hupi_h264_pack_frame → usb-stream.sock → decode/UI
```

Chi tiết format: [MEDIA.md](MEDIA.md).

---

## 4. CarPlay / Apple trên USB

### Vì sao CDC-NCM?

CarPlay wired hiện đại đi qua **mạng USB** (CDC-NCM), không qua driver tether cũ `ipheth`.

| Trạng thái | Kind (`classify.c`) | Hành động |
|---|---|---|
| Apple + NCM | `CARPLAY` | `ip link set <iface> up` → media |
| Apple, chưa NCM | `APPLE_WAIT` | Chờ kernel/driver |
| Apple + ipheth, không NCM | `APPLE_IPHETH` | **failed** (`ipheth-disabled`) |

### IAP2 / MFi

- **IAP2**: giao thức Apple Accessory Protocol 2 trên link IP.  
- **MFi**: chứng chỉ / authentication chip (proprietary).  
- Trong repo: hook `HUPI_WITH_MFI` + `libiap2` / `libhu-mfi` (chưa có trong tree — xem NEEDED.md).  
- Endpoint media mặc định lab: `10.10.10.1:5000` trên iface kiểu `usb0`.

### So sánh nhanh AA vs CarPlay

| | Android Auto | CarPlay |
|---|---|---|
| Bước USB đầu | AOA control | CDC-NCM + link up |
| Transport media | USB bulk / AOAP | IP trên NCM |
| Auth | AASDK / AA protocol | MFi + IAP2 |
| Lib trong repo | `libhu-aa` | `libhu-carplay` |

---

## 5. H.264 & frame wire

Hai magic trên `usb-stream.sock`:

| Magic | Bytes LE | Payload |
|---|---|---|
| `FRM1` | `0x314D5246` | RGB24 |
| `H264` | `0x34363248` | Annex-B AU |

Header 20 byte: magic, w, h, stride|flags, nbytes — rồi payload.  
Demo SDL **chưa decode** H.264 (chỉ đếm AU). Decode production → GStreamer / V4L2 / `hu-graphics`.

Annex-B: NAL tách bằng start code `00 00 00 01` (SPS / PPS / IDR…).

---

## 6. Unix domain socket & wire protocol

Daemons giao tiếp bằng **Unix stream socket** + **một dòng text một message** (không dùng D-Bus trong lab hiện tại).

```text
sub
dev id=1-5 vid=0x18d1 … adb=1
claim 1-5
ctrl 1-5 192 51 0 0 2
ok ctrl 0200
state backend=android phase=active streaming=1 …
```

| Khái niệm | Ý nghĩa |
|---|---|
| `sub` | Đăng ký nhận broadcast + dump snapshot |
| `key=value` | Field; giá trị escape `%XX`, rỗng → `-` |
| `hupi_peer_t` | Buffer nhận; `pull` tách theo `\n` |
| Non-blocking + `poll` | Vòng event chính của hai daemon |

Runtime dir: `/run/hupi` hoặc `$HUPI_RUNTIME` — chứa cả socket file.

---

## 7. Session policy (một backend)

`usbman` = máy trạng thái thuần (không I/O USB):

- Một projection active → máy thứ hai `parked`.  
- `stream_user`: `-1` auto, `0` tắt, `1` bật.  
- `streaming` chỉ true khi `phase=active` và user không tắt.

Queue lệnh tới driver: **tuần tự** (một `ctrl` → chờ `ok`/`err`) vì AOA phụ thuộc thứ tự.

---

## 8. SDL2 (demo UI)

Dùng cho **lab host**, không phải UI production trên EVB.

| Khái niệm SDL | Dùng trong |
|---|---|
| `SDL_Window` / `SDL_Renderer` | `usb-panel`, `hupi-cluster` |
| `SDL_Texture` RGB24 | Hiển thị frame `FRM1` |
| Event chuột/touch | Gửi `touch x y down` (tọa độ 0…10000) |
| Font bitmap (stb / custom) | `ui.c` + `hupi_font` |

Build: `-DHUPI_HOST_SDL=ON` → link `libSDL2`.  
Tắt H.264 stub nếu muốn thấy sọc RGB trên cluster: `-DHUPI_MEDIA_H264_STUB=OFF`.

---

## 9. Các khái niệm Linux khác hay gặp

| Chủ đề | Vì sao cần |
|---|---|
| **poll / non-blocking I/O** | Một vòng lặp phục vụ nhiều fd (listen, uevent, peers, stream) |
| **fork + exec `ip`** | `ip link set usb0 up` cho CarPlay (sanitize tên iface) |
| **syslog / stderr log** | `hupi_log` — TRACE trên lab, INFO trên board |
| **systemd unit** | `usb-driverd.service`, `usb-managerd.service` |
| **Yocto / BitBake** | Image Pi 4 & EVB (`meta-connectivity`, `meter-pf`) |
| **CMake options** | `HUPI_WITH_AASDK`, `HUPI_WITH_MFI`, `HUPI_MEDIA_H264_STUB` |

---

## 10. Thư viện & dependency cần có

### A) Đã dùng / bắt buộc cho lab hiện tại

| Lib / tool | Package gợi ý (Debian/Ubuntu) | Mục đích |
|---|---|---|
| **GCC + CMake** | `build-essential` `cmake` | Build C11 |
| **pthread** | glibc | `hupi_wire` |
| **Linux headers** | `linux-libc-dev` | `linux/usbdevice_fs.h`, netlink uevent |
| **SDL2** | `libsdl2-dev` | Demo panel + cluster |
| **iproute2** (`ip`) | `iproute2` | Link up iface NCM |
| **Python 3** (tuỳ) | `python3` | `tools/test-usb.py` |

Trong tree (không cần apt thêm):

| Thành phần | Path |
|---|---|
| `hupi_wire` / log / H.264 stub | `modules/common/` |
| `usbdrv` | `modules/usb-driver-userlayer/` |
| `usbman` | `modules/usb-man/` |
| `hu_aa`, `hu_carplay` | `modules/libhu-*` |
| stb / UI helper | `rasp4/build-demo/apps/common/`, `third_party/` |

### B) Cần thêm cho media thật (board)

| Lib | Cờ CMake | Vai trò |
|---|---|---|
| **AASDK** (f1xpl/aasdk hoặc fork) | `HUPI_WITH_AASDK=ON` | Android Auto protocol + video/input |
| Boost / protobuf (thường đi kèm AASDK) | (theo build AASDK) | Dependency của AASDK |
| **libiap2** | `HUPI_WITH_MFI=ON` | IAP2 trên NCM |
| **libhu-mfi** (hoặc SDK MFi HUPI) | `HUPI_WITH_MFI=ON` | Auth MFi |
| GStreamer / libav (tuỳ) | (graphics) | Decode H.264 → màn hình |
| OpenSSL / crypto (tuỳ MFi stack) | (theo SDK) | TLS / auth |

Đặt AASDK: `third_party/aasdk` (submodule). Cert MFi **không** commit vào git.

### C) Yocto / image

| Thành phần | Ghi chú |
|---|---|
| Poky + meta-raspberrypi | `rasp4/build-raspi4/upstream/` |
| `meta-connectivity` | Recipe app/lib HUPI |
| `meter-pf` | Machine EVB |

### D) Ma trận “cần gì để làm X”

| Mục tiêu | Dependency tối thiểu |
|---|---|
| Build + unit test | cmake, gcc, linux headers |
| Demo SDL + sim USB | + SDL2 |
| AOA / session trên sim | (chỉ binary trong repo) |
| Android Auto video thật | + AASDK (+ deps của nó) |
| CarPlay iPhone thật | + libiap2, libhu-mfi, cert MFi, NCM ok |
| Image flash Pi 4 | + Yocto layers |
| Decode H.264 production | + GStreamer/V4L2 (hoặc decoder board) |

---

## 11. Lộ trình học gợi ý (theo file code)

1. **USB enum / uevent** → `usb-driver-userlayer/src/enum.c`, `uevent.c`  
2. **Wire + socket** → `common/src/wire.c`, `driverd.c` `handle_line`  
3. **AOA** → `usb-man/src/aoa.c`, `driverd.c` `handle_ctrl`  
4. **Session** → `session.c`, `classify.c`  
5. **Manager loop** → `managerd.c` (`pump`, `media_sync`, stream)  
6. **Media stub** → `h264_stub.c`, `hu_aa.c` `emit_h264`  
7. **SDL demo** → `rasp4/build-demo/apps/*/main.c`  
8. **AASDK/MFi** → [MEDIA.md](MEDIA.md) + [NEEDED.md](NEEDED.md)

---

## 12. Thuật ngữ nhanh

| Thuật ngữ | Nghĩa ngắn |
|---|---|
| AOA | Android Open Accessory (control → accessory mode) |
| AOAP | Accessory mode đã active (PID Google 0x2d00/01) |
| AASDK | Android Auto SDK (open-source stack) |
| CDC-NCM | USB networking class dùng cho CarPlay |
| IAP2 | Apple Accessory Protocol 2 |
| MFi | Made for iPhone/iPad — auth proprietary |
| usbfs | User-space USB device node + ioctl |
| FRM1 / H264 | Magic frame trên usb-stream.sock |
| claim | Exclusive lease thiết bị trên usb-driverd |
