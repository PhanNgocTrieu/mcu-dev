# Hướng dẫn test USB — connectivity manager trên Raspberry Pi 4

Module được test là `connectivity-manager/usb`, binary trên board là `/usr/bin/connectivity-usb`.
Đây là phần USB nằm trong connectivity manager: nghe cắm/rút, phân loại, mount ổ đĩa, và báo cho service Android Auto / CarPlay. Nó không vẽ UI và không chạy protocol video.

Hai cây `usb-manager/` và `usb-manager-c/` là bản cũ. Daemon cũ tự mở session Android Auto demo ngay khi nhận điện thoại. Bản này không làm vậy. Khi trên Pi chỉ dùng image `core-image-connectivity` hiện tại, service chạy là `connectivity-usb`.

## Phần nào test được thật, phần nào chỉ là demo

| Hạng mục | Trên Pi 4 | Giới hạn |
| --- | --- | --- |
| Cắm / rút, phân loại Android, iPhone, ổ đĩa, HID | Chạy thật qua udev | Cắm vào cổng USB-A. Cổng USB-C dùng để cấp nguồn |
| Mount / unmount mass storage | Chạy thật (`mount` syscall) | Chỉ partition đầu. Filesystem thử lần lượt vfat, exfat, ntfs3, ext4. Kernel phải có filesystem đó |
| Probe AOAP `GET_PROTOCOL` và tìm iface NCM/RNDIS | Chạy thật (libusb + `/sys/class/net`) | Không gửi lệnh chuyển điện thoại sang accessory mode. MTP vẫn được coi là `ready` |
| `ProjectionAvailable` | Signal D-Bus thật | Service Android Auto / CarPlay bên ngoài chưa có trong image. Mình tự gọi `StartSession` để đóng vai service đó |
| `StartSession` Android Auto | Giữ chỗ phiên exclusive | Demo. Log `aa_claimed_for_external_service`. Không có video, audio, hay AASDK |
| `StartSession` CarPlay | Nhận diện VID Apple `05ac` là thật | `start` luôn trả `MFI_REQUIRED`. Không có iAP2 / chip MFi |
| Selftest trên máy dev | Không cần USB | Mount được thay bằng hàm giả. Không chứng minh được kernel hay dây cáp |

## Đọc source trước khi cầm dây

Luồng một thiết bị đi qua các file này:

```text
hotplug.c          udev add / change / remove, đọc VID/PID/serial
classifier.c       gán type: android, iphone, mass_storage, hid, unknown
manager.c          state machine
storage.c          mount ổ đĩa vào /run/connectivity/usb/<id>
transport.c        AOAP GET_PROTOCOL và quét NCM/RNDIS
adapter.c          claim phiên khi service gọi StartSession
dbus_sdbuspp.cpp   D-Bus, file C++ duy nhất
main.c             vòng poll 500 ms, systemd gọi file này
```

State machine của thiết bị, trong `include/types.h`:

```text
enumerating -> probing -> ready
                 |
                 +-> failed     (mount lỗi, hoặc probe không ra Android/iPhone)
hid/unknown -> ignored
rút cáp     -> disconnecting, rồi xóa khỏi danh sách
```

`ready` nghĩa là “thiết bị còn cắm và connectivity manager đã hiểu nó”. Với điện thoại, `ready` không có nghĩa là Android Auto đã chiếu màn hình. Session chỉ sang `active` sau khi có lệnh `StartSession`.

D-Bus, khai báo trong `include/dbus_api.h`:

| | |
| --- | --- |
| Service | `org.example.connectivity` |
| Path | `/org/example/connectivity/usb` |
| Interface | `org.example.connectivity.Usb1` |
| Method | `ListDevices`, `StartSession(device_id, mode)`, `StopSession(device_id)` |
| Signal | `DeviceChanged`, `SessionChanged`, `ProjectionAvailable` |
| Property | `ActiveSession` |

`mode` là `android_auto`, `carplay`, hoặc `storage`. `storage` bị từ chối nếu gọi `StartSession`: ổ đĩa được mount sẵn, không qua session.

`device_id` ưu tiên `usb-<vid>-<pid>-<serial>`, ví dụ `usb-18d1-4ee1-ABCDEF`. Không có serial thì dùng tên cổng vật lý. `busnum`/`devnum` không được dùng làm id vì kernel tái sử dụng sau khi rút.

## 1. Test logic trên máy dev — không cần board

Chạy tại repo trên máy build:

```bash
./scripts/build-connectivity-usb.sh
```

Script cmake, rồi chạy `connectivity-usb-selftest`. Kỳ vọng dòng cuối: `All checks passed`.

Selftest chứng minh các quyết định trong code, không đụng USB của máy:

- VID `18d1` là Android, `05ac` là iPhone, class `08` là ổ đĩa, class `03` là HID.
- Interface vendor-specific `FF` một mình không bị gọi là Android.
- Điện thoại vào `ready` và bắn callback projection. Session vẫn rỗng cho đến khi gọi start.
- CarPlay start trả `MFI_REQUIRED`, điện thoại vẫn `ready`.
- Một session Android Auto đang chiếm chỗ thì CarPlay bị `exclusive_session_active`.
- Ổ đĩa “mount” qua hàm giả, event `change` không mount lần thứ hai, rút thì “unmount”.

WSL thường không thấy USB cắm vào PC. Đừng dùng máy dev để kết luận cắm/rút. Việc đó làm trên Pi.

## 2. Chuẩn bị Raspberry Pi 4

Image cần có gói `connectivity-usb`. File sau khi bitbake:

```text
yocto/build/tmp/deploy/images/raspberrypi4-64/core-image-connectivity-raspberrypi4-64.rootfs.wic.bz2
```

Ghi thẻ bằng Raspberry Pi Imager trên Windows nếu đang ở WSL. Cấp nguồn 5 V / 3 A. Serial console là GPIO 14/15, 115200, nếu cần lúc chưa có mạng.

Đăng nhập:

```bash
ssh root@<địa-chỉ-ip>
```

Password root trống ở image prototype (`debug-tweaks`).

Cắm điện thoại và USB stick vào cổng USB-A (cổng xanh USB 3 hoặc cổng đen USB 2). Đừng cắm vào USB-C: cổng đó là nguồn, không phải cổng host của bài test này.

Kiểm tra daemon do systemd mở:

```bash
systemctl status connectivity-usb
journalctl -u connectivity-usb -b --no-pager
```

`Active: active (running)` nghĩa là `/usr/bin/connectivity-usb` đang poll udev. Unit nằm ở `systemd/connectivity-usb.service`: sau `systemd-udevd` và `dbus`, restart nếu crash, thư mục runtime `/run/connectivity/usb`.

Nếu service không có mặt, image đang là bản cũ chỉ có `usb-manager`. Flash lại image có `connectivity-usb` trước khi làm các bước dưới.

## 3. Xem kernel đã thấy thiết bị chưa

Hai lệnh này đứng ngoài daemon. Chúng trả lời “phần cứng và udev có ổn không” trước khi đổ lỗi cho state machine.

```bash
lsusb
udevadm monitor --udev --subsystem-match=usb
```

`lsusb` liệt kê thiết bị kernel đã enumerate. Trong monitor, cắm và rút một thiết bị. `add` rồi `remove` trên `usb_device` nghĩa là host port sống. Thoát monitor bằng Ctrl+C.

Nếu `lsusb` trống sau khi cắm: kiểm tra cáp (nhiều cáp sạc không có data), nguồn, và đúng cổng USB-A. Daemon không thể phân loại một thiết bị mà kernel chưa thấy.

## 4. Theo dõi log và D-Bus

Mở hai SSH, hoặc hai terminal.

Terminal log:

```bash
journalctl -u connectivity-usb -f
```

Mỗi dòng là quyết định của daemon: hotplug, type, VID/PID, mount, claim session. `--debug` không bật trong unit. Muốn log debug thì dừng service và chạy tay, xem mục 8.

Terminal D-Bus:

```bash
busctl monitor org.example.connectivity
```

`busctl` mặc định nói system bus. Đúng bus mà daemon đăng ký khi systemd khởi động.

Ba signal:

- `DeviceChanged(device_id, state, type)` — mỗi lần state đổi.
- `ProjectionAvailable(device_id, mode)` — điện thoại vừa `ready`. Service Android Auto hoặc CarPlay (chưa có trên image) sẽ nghe signal này rồi gọi `StartSession`. Trong bài test, chính mình gọi lệnh đó.
- `SessionChanged(device_id, session_state, reason)` — chỉ khi có start/stop/unplug lúc đang có phiên.

Đọc danh sách đang cắm:

```bash
busctl call org.example.connectivity \
  /org/example/connectivity/usb \
  org.example.connectivity.Usb1 \
  ListDevices
```

Mỗi mục có `device_id`, `type`, `state`, `vid`, `pid` (hex), `serial`, `mount_point`, `block_dev`, `aoap`, `ncm`, `detail`. Copy `device_id` cho lệnh start/stop.

Phiên đang chiếm chỗ:

```bash
busctl get-property org.example.connectivity \
  /org/example/connectivity/usb \
  org.example.connectivity.Usb1 \
  ActiveSession
```

Khi không có phiên projection, giá trị là `"none"`.

## 5. Test trên board

Làm lần lượt. Mỗi mục ghi điều kiện đậu.

### 5.1 HID hoặc thiết bị lạ — bị bỏ qua

Cắm chuột hoặc bàn phím USB.

Kỳ vọng trong log: `type=hid` hoặc `unknown`, state `ignored`. `ListDevices` vẫn có thiết bị, `state` là `ignored`. Không có `ProjectionAvailable`. Không có thư mục mới trong `/run/connectivity/usb`.

`ignored` là cố ý: class HID `03` không phải điện thoại và không phải ổ đĩa.

### 5.2 USB stick — mount thật

Dùng stick FAT32 cho lần đầu. Code thử `vfat` trước, rồi exfat, ntfs3, ext4. Image chưa khai báo module exfat/ntfs trong `connectivity-usb.cfg`, nên stick exFAT/NTFS có thể `failed` dù phân loại đúng.

Xem kernel đang hỗ trợ filesystem nào:

```bash
grep -E 'vfat|exfat|ntfs|ext4' /proc/filesystems
```

Cắm stick. Kỳ vọng:

1. Log hotplug `type=mass_storage`.
2. `ListDevices` có `state` `ready`, `block_dev` kiểu `/dev/sda1`, `mount_point` kiểu `/run/connectivity/usb/usb-0781-5567-<serial>`.
3. File trên stick đọc được:

```bash
ls "/run/connectivity/usb/usb-<vid>-<pid>-<serial>"
findmnt | grep connectivity
```

4. Rút stick. `findmnt` không còn dòng đó. `ListDevices` không còn id này. Log có unmount.

`change` (udev đổi property, không phải rút) không mount thêm lần nữa. Selftest đã kiểm tra nhánh này bằng hàm giả; trên board chỉ cần thấy một điểm mount cho một stick.

Nếu `state` là `failed` và `detail` chứa `mount_failed` hoặc `no_block_device`:

- `no_block_device`: kernel chưa tạo `/sys/block` dưới đúng syspath. `dmesg | tail` xem `usb-storage` có lỗi không.
- `mount_failed_errno=19` (`ENODEV`): kernel không có filesystem vừa thử. Format lại FAT32, hoặc nạp module đúng tên `vfat`/`exfat`/`ntfs3`.

### 5.3 Điện thoại Android — nhận diện thật, session là demo

Cắm điện thoại, chọn chế độ File transfer / MTP nếu máy hỏi. MTP đủ để phân loại. AOAP có thể chưa bật.

Kỳ vọng:

1. `ProjectionAvailable` với mode `android_auto`.
2. `ListDevices`: `type` `android`, `state` `ready`, `detail` dạng `aoap=...; ncm=...`.
3. `ActiveSession` vẫn `"none"`. Daemon không tự gọi start.

Đọc `detail`:

| Chuỗi | Nghĩa |
| --- | --- |
| `aoap_protocol=<n>` | Điện thoại trả lời `GET_PROTOCOL`, hỗ trợ accessory |
| `already_in_aoap_mode` | VID Google `18d1` và PID `2d00`–`2d05`, đang ở mode accessory |
| `open_failed_need_permissions_or_device_busy` | libusb không mở được node. Thường là quyền. Daemon trong `plugdev`; xem `ls -l /dev/bus/usb/...` |
| `get_protocol_failed_rc=<n>` | Mở được máy nhưng request 51 không trả version. Máy còn ở MTP là bình thường |
| `no_ncm_rndis_iface` | Chưa có card mạng USB. Bình thường lúc còn MTP |
| `matched_syspath` / tên iface | Có `usb0` (hoặc tương đương) đúng cây sysfs của máy này |

Điện thoại vẫn `ready` khi AOAP và NCM đều trống. Đó là chủ ý trong `adapter.c`: service Android Auto bên ngoài mới quyết định lúc nào mở protocol.

Đóng vai service đó:

```bash
busctl call org.example.connectivity \
  /org/example/connectivity/usb \
  org.example.connectivity.Usb1 \
  StartSession ss "usb-18d1-4ee1-SERIAL" android_auto
```

Thay id bằng đúng chuỗi trong `ListDevices`.

Kỳ vọng demo, không phải hình Android Auto:

- Lệnh trả `b true`.
- Log: `Android Auto session claimed; protocol stays in the AA service`.
- `DeviceChanged` state `active`.
- `SessionChanged` reason `aa_claimed_for_external_service`.
- `ActiveSession` dạng `"usb-...:android_auto:active"`.
- Không có cửa sổ video, không có audio. Image không chứa AASDK.

Dừng phiên:

```bash
busctl call org.example.connectivity \
  /org/example/connectivity/usb \
  org.example.connectivity.Usb1 \
  StopSession s "usb-18d1-4ee1-SERIAL"
```

Điện thoại về `ready`, `ActiveSession` về `"none"`, log `Android Auto session released`. Điện thoại vẫn cắm.

Rút cáp khi đang `active`: `SessionChanged` reason `unplug`, thiết bị biến khỏi `ListDevices`. Cắm lại thì lại `ready` và `ProjectionAvailable`, session không tự sống lại.

### 5.4 iPhone — nhận diện thật, CarPlay là stub

Cắm iPhone. VID Apple trong `lsusb` là `05ac`.

Kỳ vọng:

1. `ProjectionAvailable` mode `carplay`.
2. `state` `ready`, `detail` `apple_present_mfi_required`.
3. Log: `CarPlay candidate ready, MFi not available in this prototype`.
4. Chưa có session lỗi. Stub chỉ thất bại khi service gọi start.

```bash
busctl call org.example.connectivity \
  /org/example/connectivity/usb \
  org.example.connectivity.Usb1 \
  StartSession ss "usb-05ac-....-SERIAL" carplay
```

Kỳ vọng: lệnh lỗi, message `MFI_REQUIRED`. Log warn `CarPlay start rejected: MFI_REQUIRED`. `state` vẫn `ready`. `ActiveSession` vẫn `"none"`. Lỗi này là hợp đồng của prototype, không phải cáp hỏng.

### 5.5 Exclusive session và ổ đĩa chạy cạnh

Giữ điện thoại Android ở `active` (mục 5.3). Cắm iPhone, gọi `StartSession` carplay.

Kỳ vọng lỗi `exclusive_session_active:<id android>`. iPhone vẫn `ready`. Android vẫn `active`.

Cắm USB stick trong lúc Android Auto đang claim. Stick vẫn được mount. `ActiveSession` vẫn là android. Ổ đĩa không đi qua exclusive policy.

Gọi `StartSession` với mode `storage` trên id của stick. Kỳ vọng lỗi `storage_is_mounted_by_usb_manager`.

### 5.6 Allowlist

Chỉ khi cần chặn máy lạ. Dừng unit và chạy một process:

```bash
systemctl stop connectivity-usb
/usr/bin/connectivity-usb --allowlist SERIAL_ĐƯỢC_PHÉP --debug
```

`SERIAL_ĐƯỢC_PHÉP` là chuỗi serial USB, không phải `device_id` đầy đủ. Máy có serial đó vẫn start được. Máy khác nhận `not_in_allowlist` lúc `StartSession`. Phân loại và `ProjectionAvailable` vẫn xảy ra; allowlist chặn lúc mở phiên, không chặn lúc cắm.

Ctrl+C để thoát, rồi:

```bash
systemctl start connectivity-usb
```

Hai process không được cùng giữ tên `org.example.connectivity`.

## 6. Chạy daemon bằng tay khi debug

```bash
systemctl stop connectivity-usb
/usr/bin/connectivity-usb --debug --no-auto-enum
```

`--no-auto-enum` bỏ qua thiết bị đã cắm lúc khởi động. Chỉ sự kiện cắm sau đó mới vào state machine. Bỏ cờ này nếu muốn thấy luôn stick đang cắm sẵn.

`--no-dbus` tắt IPC. Log hotplug vẫn có, `busctl` sẽ không thấy service. Dùng khi chỉ cần xác nhận udev và phân loại.

Stdout/stderr của process tay không đi vào journal. Nhìn ngay terminal đó.

## 7. Việc chưa test được trên board này

- Chiếu màn Android Auto, audio, input. Cần service Android Auto riêng và AASDK. `StartSession` hôm nay chỉ chiếm chỗ exclusive.
- Chuyển điện thoại sang AOAP accessory (control transfer “start accessory”). Probe chỉ hỏi protocol.
- CarPlay thật. Cần MFi và stack được cấp phép. Stub cố ý trả `MFI_REQUIRED`.
- Nhiều partition trên một stick. Chỉ `/dev/sdX1` (hoặc cả đĩa nếu không có partition 1).
- UI. Không có client HMI trong image. `busctl monitor` là UI tạm của bài test.
