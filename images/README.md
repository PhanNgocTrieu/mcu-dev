# Flash images (artifact sẵn flash)

Thư mục này chứa **file image đã build**, không chứa cây Yocto.

## Có nặng không?

| Thành phần | Kích thước điển hình | Đưa vào git? |
|---|---|---|
| `rasp4/build-raspi4/build/` (Yocto tmp) | **hàng chục GB** (vd. ~60GB+) | **Không** — đã ignore trong `.gitignore` |
| `downloads/`, `sstate-cache/` | GB → TB | **Không** |
| **Image flash Pi 4** (`*.wic.bz2`) | **~90–200 MB** nén | **Có thể** — đặt dưới `images/pi4/` |
| Image EVB (`meter-pf`) | tuỳ MACHINE | Tương tự → `images/meter-pf/` (khi có) |

File `.wic.bz2` hiện tại trong lab (~92 MB) **nhẹ hơn nhiều** so với build tree, nhưng vẫn là binary lớn: mỗi lần đổi image làm repo phình. Nếu team flash thường xuyên, cân nhắc **Git LFS** hoặc **GitHub Release** thay vì commit trực tiếp.

## Cấu trúc

```text
images/
  README.md          ← file này
  pi4/
    *.wic.bz2        ← image Raspberry Pi 4 (flash)
    *.wic.bmap       ← optional, cho `bmaptool copy`
  meter-pf/          ← dành cho EVB (khi có recipe image)
    .gitkeep
```

## Đưa image vào đây sau khi build

```sh
./rasp4/build-raspi4/build-image.sh
./rasp4/build-raspi4/publish-image.sh
```

Script copy (hoặc hardlink nếu cùng filesystem) từ  
`rasp4/build-raspi4/build/tmp/deploy/images/raspberrypi4-64/`  
vào `images/pi4/`, kèm symlink `latest.wic.bz2` trỏ bản mới nhất.

## Flash SD

Ưu tiên image trong repo:

```sh
./rasp4/build-raspi4/flash-sd.sh /dev/sdX
```

(`flash-sd.sh` tìm `images/pi4/latest.wic.bz2` trước, rồi mới deploy Yocto.)

## Commit image lên git

Chỉ add file dưới `images/` — **không** add `rasp4/build-raspi4/build/`:

```sh
git add images/pi4/*.wic.bz2 images/pi4/*.wic.bmap
git status   # kiểm tra không lỡ stage GB build tree
```

GitHub cảnh báo file >50 MB; giới hạn cứng ~100 MB/file. Image Pi 4 nén thường vẫn nằm dưới ngưỡng đó.
