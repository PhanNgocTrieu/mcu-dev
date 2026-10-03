# Media & H.264 — trạng thái và việc cần làm

## Hiện tại đang có gì?

| Đường | Khi nào | Payload |
|---|---|---|
| RGB shim (`FRM1`) | `HUPI_MEDIA_H264_STUB=OFF` và chưa AASDK/MFi | Header 20 B + RGB24 640×360 |
| **H.264 stub** (`H264`) | Mặc định `HUPI_MEDIA_H264_STUB=ON` (hoặc có AASDK/MFi flag) | Header 20 B + Annex-B AU hardcode |
| AASDK / MFi thật | Chưa — chỉ hook + stub | Sẽ thay AU stub |

Stub AU nằm trong `modules/common/src/h264_stub.c` (marker ASCII `HUPI-H264-STUB` trong NAL IDR).  
Đóng gói: `hupi_h264_pack_frame()` / `hupi_h264_pack_stub()`.

### Format wire (usb-stream.sock)

Header **20 byte**, little-endian:

| Offset | RGB (`FRM1`) | H.264 (`H264`) |
|---|---|---|
| 0 | magic `0x314D5246` | magic `0x34363248` |
| 4 | width | width |
| 8 | height | height |
| 12 | stride (= w×3) | flags (bit0 = keyframe) |
| 16 | nbytes | nbytes (= độ dài AU) |
| 20… | RGB24 | Annex-B AU |

Callback media: `on_video(data, len, pts_us, is_h264, user)` — `is_h264=1` khi magic H264.

Demo `hupi-cluster` **chưa decode** H.264: hiện badge `H264` + đếm số AU. Decode thật → `hu-graphics` / GStreamer.

## Bật / tắt stub

```sh
# Mặc định ON — dùng H.264 stub trên Android Auto & CarPlay
cmake -S . -B build -DHUPI_HOST_SDL=ON

# Về RGB shim (sọc màu như trước)
cmake -S . -B build -DHUPI_HOST_SDL=ON -DHUPI_MEDIA_H264_STUB=OFF
```

Log khi stub chạy:

```text
aa.video h264 stub frame=… bytes=…
media.status phase=active detail=h264-stub
```

## Việc bạn cần làm tiếp (theo thứ tự)

### 1) Android Auto — thay stub bằng AASDK

1. Clone AASDK vào `third_party/aasdk` (f1xpl/aasdk hoặc fork nội bộ).
2. Build với `-DHUPI_WITH_AASDK=ON`.
3. Mở `modules/libhu-aa/src/hu_aa.c` → hàm `emit_h264()`:

```c
#if HUPI_WITH_AASDK
    /* Real path: pull H.264 AUs from aasdk::messenger and forward them. */
    /* TODO: thay đoạn dùng hupi_h264_stub_au() bằng AU thật từ messenger */
#endif
```

4. Giữ `hupi_h264_pack_frame(out, …, keyframe, au, au_len)` — **đừng đổi layout header**.
5. Touch: inject vào input channel AASDK trong `hu_aa_touch()`.
6. Kiểm tra log: `aasdk=1`, `detail=aasdk`, cluster đếm AU tăng.

### 2) CarPlay — thay stub bằng media TCP / MFi

1. Có `libiap2` + `libhu-mfi` (hoặc SDK HUPI).
2. `-DHUPI_WITH_MFI=ON`.
3. Trong `hu_carplay_poll()`: thay `hupi_h264_stub_au` bằng đọc socket media trên iface NCM.
4. Endpoint mặc định `10.10.10.1:5000` — chỉnh nếu khác (xem NEEDED.md).

### 3) Decode trên màn hình

- Lab: có thể tạm `ffmpeg`/`gst-launch` đọc từ file dump, hoặc mở rộng cluster.
- Board: pipeline GStreamer / V4L2 trong `hu-graphics` nhận magic `H264`.

### 4) Thay AU stub bằng clip thật (không cần AASDK)

Nếu muốn stub “đẹp hơn” trước khi có AASDK:

1. Encode một clip ngắn Annex-B:  
   `ffmpeg -f lavfi -i testsrc=size=640x360:rate=10 -t 2 -c:v libx264 -f h264 /tmp/stub.h264`
2. Chuyển thành mảng C (xxd -i) và thay `k_h264_stub_au[]` trong `h264_stub.c`.
3. Hoặc đọc file runtime (mở rộng API sau).

## Checklist nhanh

- [x] Wire magic `H264` + packer dùng chung
- [x] libhu-aa / libhu-carplay emit stub, `is_h264=1`
- [x] usb-managerd forward xuống stream + log
- [x] cluster nhận diện H264 (chưa decode)
- [ ] Nối aasdk::messenger → `emit_h264`
- [ ] Nối CarPlay media TCP → `hu_carplay_poll`
- [ ] Decode H.264 trên graphics / cluster
