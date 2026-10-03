# Thông tin cần cung cấp và hạng mục cần phát triển

Tài liệu này ghi lại những gì **chưa có trong repo** hoặc **chưa bật trên image board**. Khi có đủ input, bật cờ tương ứng trong `meter-pf` / Yocto.

## 1) Bạn cần cung cấp

| Hạng mục | Mục đích | Gợi ý / vị trí đặt |
|---|---|---|
| Tree **AASDK** (f1xpl/aasdk hoặc fork nội bộ) | Android Auto media thật trên board | `third_party/aasdk/` (git submodule) |
| **libiap2** + **libhu-mfi** (hoặc SDK CarPlay/MFi của HUPI) | Xác thực MFi + IAP2 + media CarPlay | `modules/` hoặc recipe trong `meta-connectivity/recipes-conn-lib/` |
| Cert / provisioning **MFi** (nếu có) | Chạy CarPlay với iPhone thật | Không commit secret; đưa qua biến/local conf riêng |
| Xác nhận **MACHINE EVB** (tên machine Yocto meter-pf) | Image chính thức khác `raspberrypi4-64` | `meter-pf/conf/` |
| Panel / display EVB (HDMI vs DSI, xoay) | Weston / hu-graphics | `meta-connectivity/conf/display/` |
| Endpoint CarPlay thật (IP:port, nếu khác `10.10.10.1:5000`) | `libhu-carplay` | cấu hình runtime / recipe |
| Chính sách: AA open-source trước, CarPlay proprietary sau? | Ưu tiên tích hợp | quyết định product |

## 2) Cần phát triển tiếp (code / Yocto)

### Media thật
- [x] Wire H.264 (`magic H264`) + AU stub hardcode (`hupi_h264_*`) — xem [MEDIA.md](MEDIA.md)
- [ ] `libhu-aa`: thay `hupi_h264_stub_au` bằng AU từ aasdk::messenger (AOAP bulk / video + input)
- [ ] `libhu-carplay`: nối IAP2/MFi, TCP media trên NCM, forward AU thật thay stub
- [ ] Recipe Yocto `aasdk`, `libiap2`, `libhu-mfi` + `EXTRA_OECMAKE:pn-libhu-aa/carplay=ON` trên `meter-pf`
- [ ] Decode frame H.264 trong `hu-graphics` (GStreamer / V4L2); cluster hiện chỉ đếm AU

### Module còn thiếu so với sơ đồ HUPI
- [ ] `hu-connectivity` (session manager tổng), `hu-touch`, `hu-graphics` tách process
- [ ] `libhu-bus` (D-Bus / FD passing như sơ đồ) thay unix-line protocol lab
- [ ] Gadget / USB leasing nâng cao nếu EVB cần dual-role

### Board / lab
- [ ] Image `meter-pf` (MACHINE EVB) dùng chung `modules/` + `meta-connectivity`
- [ ] Rebuild `rasp4/build-raspi4` sau khi có AASDK/MFi và flash lại Pi 4
- [ ] Kiểm thử điện thoại thật: Android AOA → AA video; iPhone NCM → CarPlay (sau MFi)

### Chất lượng
- [ ] Mở rộng Doxygen (file `.c` còn lại) + trang `docs/` HTML nếu cần
- [ ] Log: giữ TRACE trên lab; mặc định INFO trên image production
- [ ] CI host: `ctest` + `tools/test-usb.py`

## 3) Cờ build đã chuẩn bị sẵn

```bitbake
EXTRA_OECMAKE:pn-libhu-aa = "-DHUPI_WITH_AASDK=ON"
EXTRA_OECMAKE:pn-libhu-carplay = "-DHUPI_WITH_MFI=ON"
```

Host / demo: mặc định `HUPI_MEDIA_H264_STUB=ON` (AU hardcode). Tắt: `-DHUPI_MEDIA_H264_STUB=OFF` → RGB shim.

## 4) Kiểm tra log

Không AASDK, stub ON:

```text
media.start android ... aasdk=0
media.status phase=active detail=h264-stub
aa.video h264 stub frame=… bytes=…
```

Stub OFF (RGB):

```text
media.status phase=active detail=shim
```

Khi đã link đúng: `aasdk=1` / `detail=aasdk` và `mfi=1` / `detail=mfi`.
