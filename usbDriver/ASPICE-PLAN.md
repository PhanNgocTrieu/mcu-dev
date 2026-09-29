# Kế hoạch ASPICE — module USB-driver (RA/IA đến SWE.6)

Tài liệu này là kế hoạch làm việc cho module **USB-driver**. Tài liệu kỹ thuật đi kèm: [USB-DRIVER-SOURCE.md](USB-DRIVER-SOURCE.md).

Cặp chữ **RA/IA** trong kế hoạch này là Requirements Analysis và Impact Analysis. Nếu template nội bộ viết RA/AI, nội dung gói việc giữ nguyên, chỉ đổi tên mục trên trang bìa.

Thuật ngữ ASPICE dùng trong kế hoạch nằm ở [mục 16](#16-thuật-ngữ-aspice). Thuật ngữ USB (probe, HCD, NCM, …) nằm ở [USB-DRIVER-SOURCE.md, mục 16](USB-DRIVER-SOURCE.md#16-thuật-ngữ-usb).

## 1. Kết luận phạm vi

USB-driver trong dự án Meter là **tích hợp USB Host của Linux**, pin theo phiên bản kernel của image. Source giao thức nằm ở upstream. Phần việc công ty sở hữu là yêu cầu component, kiến trúc biên, cấu hình kernel, device tree khi có board, và bằng chứng kiểm thử tại biên kernel.

Baseline đọc source cho kế hoạch này: **Linux v6.18** (`git tag v6.18`). Image Raspberry Pi thực tế lấy kernel qua recipe `linux-raspberrypi` trong Yocto. Phiên bản image mới là baseline chính thức; v6.18 là mốc để đọc code và trỏ hàm. Khi hai phiên bản lệch nhau, Impact Analysis ghi delta.

Lab hiện tại: Raspberry Pi 3, Pi 4, Pi 5. Meter-EVB chưa có board. Mọi yêu cầu phụ thuộc schematic USB của EVB ở trạng thái mở, không bịa thông số.

Module **USB-Manager** (hotplug policy, phân loại điện thoại, session Android Auto / CarPlay, D-Bus) nằm ngoài SWE.1–SWE.6 của tài liệu này. USB-driver dừng ở chỗ kernel đã enumerate, bind class driver, và phát uevent / sysfs / usbfs / netdev.

Cấu hình kernel đã có trong repo:

`usbManager/meta-connectivity/recipes-kernel/linux/files/connectivity-usb.cfg`

```
CONFIG_USB=y
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_XHCI_PCI=y
CONFIG_USB_STORAGE=m
CONFIG_USB_ACM=m
CONFIG_USB_SERIAL=m
CONFIG_USB_NET_DRIVERS=m
CONFIG_USB_USBNET=m
CONFIG_USB_NET_CDCETHER=m
CONFIG_USB_NET_CDC_NCM=m
CONFIG_USB_NET_RNDIS_HOST=m
CONFIG_USB_HID=y
```

Fragment này bật xHCI qua PCI (đúng hướng cổng USB-A của Pi 4, chip VL805). Pi 3 dùng DWC2. Pi 5 dùng xHCI trên RP1, có thể là platform driver chứ không phải `xhci-pci`. Đây là hạng mục Impact Analysis ngay từ tuần đầu, trước khi coi fragment là đủ cho cả ba Pi.

## 2. Cách đánh giá phần mềm standard

Assessor ASPICE chấp nhận phần mềm không tự viết khi đủ bốn điều sau.

| Điều kiện | Cách làm với USB-driver |
|---|---|
| Có yêu cầu của sản phẩm, viết theo giọng component | SWE.1, ID `USBDRV-REQ-xxx` |
| Thành phần standard được đặt tên, pin version, nêu giao diện | SWE.2. Phần tử loại `reused` |
| Thiết kế chi tiết và unit verification chỉ cho code/config tự làm | SWE.3 / SWE.4 cho fragment, DTS, patch. Upstream được tham chiếu, không chép thành design của công ty |
| Phần standard được kiểm ở biên, trace ngược về yêu cầu | SWE.5 tích hợp stack trên board lab. SWE.6 chứng minh yêu cầu |

Patch kernel, nếu phát sinh, đổi loại: đoạn patch là phần mềm tự phát triển và đi đủ SWE.3 và SWE.4. Không patch thì SWE.3 là cấu hình và tài liệu biên.

Source Linux không vendor vào repo sản phẩm. GPL đi theo kernel của image Yocto. Máy học thì clone riêng, lệnh nằm trong tài liệu source.

## 3. Bản đồ quy trình

```text
Nhu cầu feature USB
        |
        v
  RA  +  IA          phạm vi, yêu cầu nháp, tác động board / kernel
        |
        v
  SWE.1               Software Requirements, review, trace lên feature
        |
        v
  SWE.2               Kiến trúc: HCD, usbcore, class driver, biên sang userspace
        |
        v
  SWE.3               Cấu hình, DTS, patch (nếu có). Upstream = reference
        |
        +----> SWE.4  Kiểm đơn vị phần tự viết (hoặc justification nếu chỉ có config)
        |
        v
  SWE.5               Tích hợp HCD + core + class + uevent trên Pi 3/4/5
        |
        v
  SWE.6               Qualification theo USBDRV-REQ, trên lab. EVB để blocked
```

Quy trình quanh gói này, không viết thành plan riêng trong file này: quản lý cấu hình (pin commit kernel, fragment cfg), quản lý thay đổi khi bump kernel, ghi lỗi khi test fail.

## 4. RA — Requirements Analysis

### 4.1 Mục tiêu

Biến nhu cầu USB của đồng hồ / connectivity thành yêu cầu của component USB-driver, và gắn mỗi yêu cầu với một trong ba kiểu thực hiện:

- `Standard` — Linux upstream đáp ứng, mình kiểm tra biên.
- `Owned` — mình cấu hình hoặc viết (fragment, DTS, thêm config còn thiếu).
- `Open` — phụ thuộc Meter-EVB hoặc yêu cầu hệ thống chưa giao.

### 4.2 Đầu vào

- Nhu cầu feature: USB Host, điện thoại (Android Auto, CarPlay), lưu trữ, HID, nguồn cổng.
- Ranh giới đã chốt với USB-Manager: manager nghe udev / đọc sysfs / mở libusb; không sở hữu HCD.
- Fragment `connectivity-usb.cfg`.
- Kết quả lab: `uname -r`, `lsusb -t`, `lsmod` trên từng Pi đang có.

### 4.3 Việc phải làm

1. Liệt kê actor bên ngoài component: thiết bị USB, cổng vật lý, USB-Manager, nguồn VBUS.
2. Viết yêu cầu theo mẫu ở mục 4.5. Mỗi câu là một khả năng quan sát được khi test.
3. Gắn kiểu `Standard` / `Owned` / `Open`.
4. Với mục `Standard`, cột design để trống ở giai đoạn RA, điền sau ở SWE.2 bằng đường dẫn file kernel.
5. Ghi yêu cầu **không** thuộc module. Ví dụ session video Android Auto, xác thực MFi, chính sách “chỉ một phone active”.
6. Xin review với chủ USB-Manager ở những yêu cầu có chữ uevent, sysfs, usbfs, netdev.

### 4.4 Tiêu chí thoát

- Bộ `USBDRV-REQ` có ID ổn định.
- Mỗi yêu cầu có kiểu thực hiện và cách quan sát khi pass/fail.
- Danh sách ngoài scope có chữ ký hoặc ghi nhận review với USB-Manager.
- Mục `Open` có điều kiện đóng (schematic, kernel EVB, hoặc quyết định PM cắt scope).

### 4.5 Bộ yêu cầu hạt giống

Đây là hạt giống để đưa vào SRS sau review. Chưa phải baseline đã ký.

| ID | Yêu cầu | Kiểu | Quan sát khi đạt |
|---|---|---|---|
| USBDRV-REQ-001 | Host controller của cổng feature đăng ký với usbcore và có root hub | Owned (chọn HCD) + Standard (usbcore) | `lsusb -t` có bus USB |
| USBDRV-REQ-002 | Thiết bị cắm vào cổng Host được cấp địa chỉ, đọc descriptor, chọn configuration | Standard | `dmesg` có device number; `lsusb` thấy VID/PID |
| USBDRV-REQ-003 | Interface mass-storage được bind bởi `usb-storage` và xuất block device | Standard | `lsblk` có disk; driver trong sysfs là `usb-storage` |
| USBDRV-REQ-004 | Interface CDC-NCM được bind bởi `cdc_ncm` và có network interface | Standard | `cdc_ncm` trong driver sysfs; `ip link` có iface |
| USBDRV-REQ-005 | Interface RNDIS host được bind bởi `rndis_host` | Standard | driver `rndis_host`, có netdev |
| USBDRV-REQ-006 | CDC-ACM hoặc USB serial tạo device node tty khi thiết bị khai báo class tương ứng | Standard | `/dev/ttyACM*` hoặc `/dev/ttyUSB*` |
| USBDRV-REQ-007 | HID trên USB bind được và tạo input hoặc hidraw | Standard | driver `usbhid` |
| USBDRV-REQ-008 | Cắm và rút phát uevent add/remove, có biến `PRODUCT` | Standard | `udevadm monitor` thấy add/remove |
| USBDRV-REQ-009 | sysfs xuất vendor, product, serial (nếu thiết bị có iSerial) | Standard | `/sys/bus/usb/devices/.../idVendor` |
| USBDRV-REQ-010 | Node usbfs tồn tại để userspace gửi control/bulk (libusb) | Standard | `/dev/bus/usb/BBB/DDD` |
| USBDRV-REQ-011 | Rút cáp gỡ interface, block device, netdev, tty đã tạo cho thiết bị đó | Standard | sysfs device biến mất; uevent remove |
| USBDRV-REQ-012 | Quá dòng trên cổng được kernel ghi nhận và cổng không giữ một device “sống” giả | Standard, lab hạn chế | sysfs `over_current_count` hoặc log hub; case lab ghi “không kích được trên Pi” nếu phần cứng không tạo được sự kiện |
| USBDRV-REQ-013 | Cổng feature ở Host. USB-A của Pi không bị ép gadget | Owned | `lsusb` thấy thiết bị dưới Host controller; không có gadget bound trên cổng đó |
| USBDRV-REQ-014 | Điện thoại Android enumerate. Kernel xuất interface mà lớp trên dùng cho Android Auto (NCM, RNDIS, hoặc vendor/bulk) | Standard | `lsusb -v` liệt kê interface; AOAP switch là việc userspace, ghi rõ trong điều kiện test |
| USBDRV-REQ-015 | Thiết bị Apple VID `05ac` enumerate. Kernel xuất interface có trên dây (thường NCM hoặc vendor). Phiên CarPlay và MFi thuộc module khác | Standard | `lsusb` VID `05ac`; không lấy “CarPlay Active” làm tiêu chí USB-driver |
| USBDRV-REQ-016 | Cùng một class driver và cùng dạng sysfs/uevent trên Pi 3, Pi 4, Pi 5. Tên HCD được phép khác | Owned (config từng board) | Bảng diff `lsusb -t` trong biên bản SWE.5 |
| USBDRV-REQ-017 | Enumerate lỗi thì có log kernel và không để node cũ của lần cắm trước | Standard | log “can't read configurations” hoặc disconnect; sysfs sạch sau rút |

Yêu cầu nguồn kiểu charge-only, role USB-C, dòng danh định từng cổng EVB: để `Open`, ID dự kiến `USBDRV-REQ-018` khi có schematic. Linux `usbcore` cấp/cắt `PORT_POWER` và báo over-current. Chính sách sạc và công tắc data line của Meter là phần cứng cộng userspace, chưa gán cho module này.

## 5. IA — Impact Analysis

### 5.1 Mục tiêu

Chốt cái giữ nguyên khi đổi lab board và khi sang Meter-EVB, và cái phải làm lại. IA được cập nhật lại mỗi lần bump kernel hoặc có schematic mới.

### 5.2 Bảng tác động

| Hạng mục | Pi 3 | Pi 4 | Pi 5 | Meter-EVB | Tác động lên việc làm |
|---|---|---|---|---|---|
| HCD | DWC2, `dwc2_driver_probe` | xHCI PCI, `xhci_pci_probe` | xHCI trên RP1, xác nhận `xhci-plat` hay PCI khi lab | IP USB trên SoC, chưa có | SWE.3 có config/DTS riêng từng MACHINE. Không viết lại class driver |
| Class driver (`cdc_ncm`, `rndis_host`, `usb-storage`, `cdc-acm`, `usbhid`) | Giữ | Giữ | Giữ | Giữ nếu kernel mainline và bật config | SWE.6 dùng chung test case class |
| uevent `PRODUCT` / `TYPE`, sysfs, usbfs | Giữ | Giữ | Giữ | Giữ | Hợp đồng với USB-Manager ổn định |
| Fragment hiện tại `XHCI_PCI` | Không thay DWC2 của machine config | Khớp VL805 | Chưa đủ một mình | Tùy IP | Bổ sung cfg theo board, đừng giả định một file cho mọi SoC |
| Nguồn cổng | Ngân sách dòng của Pi | USB-A Pi 4 dễ thiếu dòng với phone | Ngân sách RP1 | Có thể có boost, sạc, data switch | REQ-012 và REQ-018 mở đến khi đo được |
| Gadget / dual-role | Cổng OTG khác USB-A | USB-C là DWC2, không phải cổng feature | Kiểm tra role USB-C | Mở | REQ-013: feature port = Host |
| Android Auto | Kernel chỉ enumerate. Mode AOAP do userspace | như Pi 3 | như Pi 3 | như lab nếu phone là USB device | Không đưa protocol AASDK vào SRS của driver |
| CarPlay | Không có driver CarPlay trong tree chuẩn | như vậy | như vậy | MFi là silicon + stack bản quyền | REQ-015 dừng ở enumerate |
| Đổi version kernel | Hàm nội bộ có thể đổi dòng | như vậy | như vậy | Pin bằng recipe Yocto | IA mới: diff `drivers/usb/core`, `drivers/net/usb` trên symbol config mình dùng |

### 5.3 Quyết định IA dùng cho các SWE phía sau

- Kiến trúc SWE.2 vẽ HCD là hộp đổi theo board. `usbcore` và class driver là hộp ổn định.
- Test SWE.5 bắt buộc một lần trên mỗi Pi đang giữ trong lab, cùng checklist.
- EVB không chặn ký SWE.1/SWE.2 của phần `Standard`. EVB chặn đóng REQ `Open` và chặn tuyên bố qualification sản phẩm.
- Bump kernel là một change request: chạy lại IA, rồi chạy lại SWE.5 cho smoke set (REQ-001, 002, 008, 011, 016).

### 5.4 Tiêu chí thoát

- Bảng trên đã điền cột lab bằng log thật (không để trống Pi nào còn trong phạm vi test).
- Cột EVB ghi “mở” cùng tên dữ liệu còn thiếu: sơ đồ USB, DTS, dòng cấp VBUS.
- PM xác nhận CarPlay full và Android Auto session không nằm trong estimate USB-driver.

## 6. SWE.1 — Software Requirements Analysis

| Hạng mục | Nội dung |
|---|---|
| Mục tiêu | Chốt SRS đã review. RA là bản nháp phân tích; SWE.1 là baseline yêu cầu |
| Work product | `USBDRV-SWE1-SRS` (bảng mục 4.5 sau chỉnh), biên bản review, trace lên nhu cầu feature / system requirement nếu có ID hệ thống |
| Việc làm | Gán nguồn của từng REQ (feature ID hoặc “derived”). Thêm thuộc tính: ưu tiên, board áp dụng, tiêu chí pass. Loại câu mơ hồ (“hỗ trợ USB”, “tương thích Android”) |
| Liên kết IA | Mỗi REQ `Open` có issue ID. REQ-016 trỏ thẳng bảng IA |
| Thoát | Review không còn comment major. Trace một chiều từ feature xuống REQ đủ các nhu cầu đã nhận. Manager ký các REQ có chữ uevent/sysfs/usbfs/netdev |

Đầu ra SWE.1 là đầu vào kiến trúc và là đầu vào SWE.6. Chưa có baseline SWE.1 thì chưa viết test qualification với tư cách bằng chứng ASPICE.

## 7. SWE.2 — Software Architectural Design

### 7.1 Phần tử kiến trúc

| Phần tử | Loại | File đối chiếu Linux v6.18 | Giao tiếp ra |
|---|---|---|---|
| HCD xHCI PCI | reused, chọn bởi config | `drivers/usb/host/xhci-pci.c` `xhci_pci_probe` | `usb_add_hcd` |
| HCD DWC2 | reused, Pi 3 | `drivers/usb/dwc2/platform.c` `dwc2_driver_probe` | `usb_add_hcd` |
| HCD xHCI platform | reused, xác nhận trên Pi 5 và EVB | `drivers/usb/host/xhci-plat.c` `xhci_plat_probe` | `usb_add_hcd` |
| USB core + hub | reused | `drivers/usb/core/hub.c`, `message.c`, `driver.c`, `hcd.c` | URB xuống HCD; device model lên sysfs |
| Class drivers | reused | `drivers/net/usb/cdc_ncm.c`, `rndis_host.c`, `drivers/usb/storage/usb.c`, `drivers/usb/class/cdc-acm.c`, HID | netdev, block, tty, input |
| Cấu hình image | owned | `connectivity-usb.cfg` + DTS/board cfg | bật đúng phần tử trên |
| Biên userspace | interface, không phải module | uevent, sysfs, usbfs | USB-Manager |

Sơ đồ tĩnh và sequence đặt trong [USB-DRIVER-SOURCE.md](USB-DRIVER-SOURCE.md). SWE.2 trích dẫn tài liệu đó, không vẽ một bản lệch nội dung.

### 7.2 Hợp đồng với USB-Manager

Manager được phép phụ thuộc:

- uevent subsystem `usb`, biến `PRODUCT` và `TYPE` do `usb_uevent()` điền (`drivers/usb/core/driver.c`).
- Thuộc tính sysfs chuẩn (`idVendor`, `idProduct`, `bInterfaceClass`, serial).
- `/dev/bus/usb` cho control transfer (AOAP và mọi vendor request).
- Netdev do `cdc_ncm` hoặc `rndis_host` tạo sau khi interface class khớp.
- Log kernel khi enumerate thất bại.

Manager không phụ thuộc tên nội bộ của HCD, không gọi `usb_submit_urb` trong kernel, không cần biết Pi đang dùng DWC2 hay xHCI để phân loại VID/PID.

### 7.3 Tiêu chí thoát

- Mỗi REQ SWE.1 map tới đúng một phần tử ở bảng 7.1, hoặc map “ngoài component”.
- HĐ interface manager được chủ USB-Manager review.
- Quyết định: không thiết kế driver class mới cho Android Auto hay CarPlay.

## 8. SWE.3 — Software Detailed Design và Unit Construction

| Đơn vị tự xây | Thiết kế chi tiết cần có | Xây dựng |
|---|---|---|
| `connectivity-usb.cfg` | Bảng symbol → REQ. Ghi symbol còn phải thêm theo từng MACHINE (`USB_DWC2` cho Pi 3, HCD của Pi 5) | fragment đã có; bản cập nhật theo IA |
| Device tree / board config EVB | Node USB, `dr_mode = "host"` trên cổng feature, nguồn nếu có regulator | Khi có DTS. Trước đó đơn vị này trạng thái mở |
| Patch kernel | Chỉ khi IA chỉ ra thiếu hành vi không cấu hình được. Mô tả lý do, file, cách hoàn tác | Không mở patch “cho đủ design” |

Đơn vị upstream không có detailed design nội bộ. Ô trace SWE.3 của chúng ghi: `reused, Linux <version image>, <path>`.

Clone source để đọc, theo mục đầu của tài liệu source. Không copy `drivers/usb` vào repo Meter.

### Tiêu chí thoát

- Mọi phần `Owned` có file trong hệ build Yocto hoặc một issue “chưa build được vì thiếu board”.
- `git`/recipe chỉ ra commit kernel của image lab.
- Không có file detailed design nào chép thân hàm `hub_port_init` vào tài liệu công ty.

## 9. SWE.4 — Software Unit Verification

Phần reused không viết unit test cho `hub.c` hay `cdc_ncm.c`. Trong kế hoạch kiểm thử ghi một justification: đơn vị upstream được qualify bằng SWE.5/SWE.6 tại biên, version đã pin.

Phần owned:

| Đơn vị | Cách kiểm | Pass |
|---|---|---|
| Fragment cfg | Inspection: mỗi `CONFIG_*` có mặt và = `y` hoặc `m` đúng SRS. Build image, `zcat /proc/config.gz` hoặc `/boot/config-*` khớp | Đủ symbol REQ-003 đến REQ-007, REQ-001 |
| DTS cổng Host | Inspection `dr_mode` khi có file. Trên lab Pi dùng DTS của meta-raspberrypi, ghi “board DT không phải sản phẩm mình viết” | REQ-013 |
| Patch, nếu có | Test đơn vị của hàm thêm, cộng regression build | Tiêu chí trong design của patch |

Thoát SWE.4: biên bản inspection config, và justification cho đơn vị upstream. Thiếu unit test kernel không phải một gap nếu justification được review.

## 10. SWE.5 — Software Integration và Integration Test

Mục tiêu: HCD của đúng board, usbcore, class driver và đường lên userspace chạy cùng nhau. Chưa cần USB-Manager. Quan sát bằng `lsusb`, sysfs, `udevadm`, `ip`, `lsblk`.

Môi trường: Pi 3, Pi 4, Pi 5, image có fragment. Ghi `uname -r` trong từng biên bản.

Bộ test là phần SWE.5 trong [USB-DRIVER-SOURCE.md](USB-DRIVER-SOURCE.md) (cột mức `SWE.5`). Smoke khi đổi kernel: REQ-001, 002, 008, 011, 016.

Thứ tự tích hợp:

1. Boot, xác nhận HCD và root hub (REQ-001, REQ-013).
2. Cắm storage, xác nhận `usb-storage` (REQ-003) — thiết bị đơn giản, tách lỗi class khỏi lỗi phone.
3. Cắm phone, đọc descriptor (REQ-002, 009, 010, 014 hoặc 015).
4. Nếu phone đã ở NCM/RNDIS, xác nhận netdev (REQ-004, 005).
5. Rút cáp (REQ-011).
6. Lặp trên Pi còn lại, điền bảng diff HCD (REQ-016).

Một case fail vì thiếu dòng điện (điện thoại rớt trong lúc enumerate) ghi là lỗi nguồn lab, link IA, không đổi SRS cho đến khi đo lại bằng hub có nguồn.

### Tiêu chí thoát

- Mỗi Pi trong scope có biên bản. Case blocked ghi lý do phần cứng.
- Không có integration defect mở ở mức “không enumerate được storage” trên Pi 4.
- Kết quả Pi 3/Pi 5 nếu lệch HCD vẫn pass REQ-016 khi class và uevent đúng.

## 11. SWE.6 — Software Qualification Test

Mục tiêu: chứng minh baseline SWE.1, trên môi trường lab đại diện sản phẩm ở giai đoạn chưa có EVB.

| Quy tắc | Áp dụng |
|---|---|
| Mỗi REQ `Standard` và `Owned` có ít nhất một case SWE.6, hoặc một case SWE.5 được đánh dấu dùng lại cho qualification | Cột trace trong tài liệu source |
| REQ `Open` | Trạng thái not-run, lý do là IA, không tính là pass |
| REQ-014 | Pass khi kernel liệt kê interface của điện thoại. Pass này không có nghĩa Android Auto đã lên hình |
| REQ-015 | Pass khi VID Apple enumerate. Pass này không có nghĩa CarPlay đã chạy |
| REQ-012 | Nếu Pi không tạo được over-current, case SWE.6 = waived-on-lab, kèm phân tích rằng sự kiện nằm ở `hub_event` / `port_over_current_notify`, và sẽ chạy lại trên EVB nếu cổng có báo quá dòng |
| Đổi kernel hoặc đổi fragment | Chạy lại toàn bộ SWE.6 smoke, không chỉ case vừa sửa |

Work product: `USBDRV-SWE6-QT` (biên bản), ma trận trace REQ → thiết kế (phần tử SWE.2) → test → kết quả → board.

Thoát: không REQ `Standard`/`Owned` nào thiếu test mà chưa có waiver đã review. Baseline ghi rõ “qualified on Raspberry Pi lab, Meter-EVB pending”.

## 12. Ma trận trace (khung)

| REQ | Phần tử SWE.2 | Đơn vị SWE.3 | SWE.4 | SWE.5 / SWE.6 |
|---|---|---|---|---|
| USBDRV-REQ-001 | HCD + `usb_add_hcd` | cfg HCD theo board | inspection config | TC-HCD-001 |
| USBDRV-REQ-002 | usbcore hub | reused | justification | TC-ENUM-001 |
| USBDRV-REQ-003 | `usb-storage` | `CONFIG_USB_STORAGE` | inspection | TC-STORE-001 |
| USBDRV-REQ-004 | `cdc_ncm` | `CONFIG_USB_NET_CDC_NCM` | inspection | TC-NCM-001 |
| USBDRV-REQ-005 | `rndis_host` | `CONFIG_USB_NET_RNDIS_HOST` | inspection | TC-RNDIS-001 |
| USBDRV-REQ-006 | `cdc-acm`, `usbserial` | cfg tương ứng | inspection | TC-ACM-001 |
| USBDRV-REQ-007 | `usbhid` | `CONFIG_USB_HID` | inspection | TC-HID-001 |
| USBDRV-REQ-008 | `usb_uevent` | reused | justification | TC-UEVENT-001 |
| USBDRV-REQ-009 | sysfs usbcore | reused | justification | TC-SYSFS-001 |
| USBDRV-REQ-010 | usbfs | `CONFIG_USB` | justification | TC-USBFS-001 |
| USBDRV-REQ-011 | `usb_disconnect` | reused | justification | TC-UNPLUG-001 |
| USBDRV-REQ-012 | hub port power | reused | justification | TC-OC-001 waived hoặc chạy |
| USBDRV-REQ-013 | HCD Host, không gadget trên cổng feature | DTS + không bật gadget trên cổng A | inspection | TC-HOST-001 |
| USBDRV-REQ-014 | class driver sẵn có, AOAP ở userspace | cfg net + storage | inspection | TC-AA-001 |
| USBDRV-REQ-015 | enumerate Apple | cfg | justification | TC-CP-001 |
| USBDRV-REQ-016 | HCD theo board | cfg từng MACHINE | inspection | TC-DELTA-001 |
| USBDRV-REQ-017 | đường lỗi enumerate | reused | justification | TC-ERR-001 |

Tên case khớp tài liệu source. Khi thêm REQ, thêm một dòng vào đây trước khi viết test.

## 13. Thứ tự làm trong năm tuần lab

| Tuần | Xong cái gì | Cửa thoát |
|---|---|---|
| 1 | RA hạt giống + lab `lsusb -t` ba Pi + điền IA cột Pi | Bảng IA có log. PM xác nhận ngoài scope AA session / MFi |
| 2 | Review SWE.1. Dựng SWE.2 từ tài liệu source. Khóa hợp đồng với USB-Manager | SRS reviewed. Interface reviewed |
| 3 | SWE.3: đối chiếu fragment với SRS, ghi symbol thiếu theo từng Pi. SWE.4 inspection | Config build ra đúng symbol trên ít nhất Pi 4 |
| 4 | SWE.5: storage, phone, unplug, delta board | Biên bản ba Pi hoặc Pi nào blocked thì ghi thiếu phần cứng |
| 5 | SWE.6 trên cùng bằng chứng, waiver REQ-012/018, ma trận trace | Gói review được gửi. Qualification ghi “lab Pi, EVB pending” |

EVB đến sau tuần 5 thì mở lại IA, SWE.3 (DTS), và các case blocked. Không viết lại SRS phần `Standard` trừ khi hành vi biên (sysfs, class) thực sự khác.

## 14. Việc cố ý không làm

- Không viết driver USB mới cho Android Auto hoặc CarPlay.
- Không đưa sequence AASDK, iAP2, HMI vào tài liệu USB-driver.
- Không unit-test source upstream.
- Không ước lượng công như một stack USB tự phát triển từ `hub.c`.
- Không đóng qualification sản phẩm khi cột EVB còn mở.

## 15. Gói nộp cho buổi review

1. SRS (SWE.1) từ bảng mục 4, sau review.
2. Impact Analysis mục 5, có log lab đính kèm.
3. Kiến trúc: trỏ sang USB-DRIVER-SOURCE.md, cộng biên bản review interface.
4. Inspection config (SWE.4) và justification reused.
5. Biên bản SWE.5 và SWE.6, ma trận mục 12 đã điền kết quả.
6. Danh mục mở: schematic Meter-EVB, HCD Pi 5 đã xác nhận bằng `lsusb -t`, kernel version image đã pin.

## 16. Thuật ngữ ASPICE

Các từ dưới là nghĩa khi dùng trong kế hoạch USB-driver. Tên SWE.1 đến SWE.6 trong tài liệu này theo Automotive SPICE 3.1. PAM 4.0 đổi SWE.5 thành Software Component Verification and Integration Verification, và SWE.6 thành Software Verification. Nội dung việc làm của module không đổi. Nếu assessor dùng 4.0, đổi tên work product cho khớp PAM, giữ ID yêu cầu.

### Khung chung

| Thuật ngữ | Nghĩa trong task này |
|---|---|
| ASPICE | Automotive SPICE. Mô hình đánh giá năng lực quy trình phần mềm ô tô. Với USB-driver, nó quy định tài liệu và bằng chứng nào phải có, không quy định phải tự viết lại driver Linux |
| Process | Một quy trình có mục đích và đầu ra. Chuỗi mình làm là SWE.1 đến SWE.6, cộng RA/IA phía trước |
| Work product | Sản phẩm của quy trình: SRS, bản IA, sơ đồ kiến trúc, biên bản test. Một file markdown có thể chứa nhiều work product, miễn là review tách được |
| Outcome | Kết quả quy trình phải đạt. Ví dụ outcome của SWE.1 là yêu cầu đã được phân tích, review, và trace được |
| Base practice (BP) | Việc cụ thể trong một process mà assessor hỏi “đã làm chưa”. Ví dụ với SWE.1: xác định yêu cầu, đánh giá mức đúng, bảo đảm trace. Kế hoạch này gói các BP đó thành mục “việc phải làm” và “tiêu chí thoát” |
| Capability level | Mức năng lực. CL1 là quy trình được thực hiện và có đầu ra. CL2 thêm quản lý (kế hoạch, theo dõi, work product kiểm soát). CL3 thêm quy trình chuẩn của tổ chức. Task USB-driver tạo bằng chứng ở mức thực hiện. Mức CL2/CL3 phụ thuộc cách công ty quản lý dự án, không tạo thêm bằng cách viết thêm sequence kernel |
| Baseline | Bản đã review và được dùng làm mốc. SRS sau review SWE.1 là baseline yêu cầu. Kernel image lab là baseline kỹ thuật. Sửa sau baseline đi qua IA hoặc change request |
| Review | Người liên quan đọc và ghi nhận, không chỉ tác giả tự ký. Với module này, USB-Manager review các yêu cầu và interface có uevent, sysfs, usbfs, netdev |
| Traceability | Liên kết hai chiều: từ nhu cầu feature xuống REQ, từ REQ sang phần tử kiến trúc, sang config, sang test, và ngược lại. Khung nằm ở mục 12 |
| Change request | Yêu cầu đổi sau khi đã có baseline. Bump kernel hoặc có schematic EVB là change request: cập nhật IA rồi chạy lại smoke test |
| Assessor | Người đánh giá ASPICE. Họ lấy work product và hỏi trace, không đọc hộ `hub.c` |

### RA / IA

| Thuật ngữ | Nghĩa trong task này |
|---|---|
| RA (Requirements Analysis) | Phân tích nhu cầu USB thành yêu cầu của component. Bản hạt giống là bảng mục 4.5. RA chưa phải baseline cho đến khi qua review SWE.1 |
| IA (Impact Analysis) | Phân tích cái gì đổi nếu đổi board hoặc đổi kernel, cái gì giữ (class driver, uevent). Bảng mục 5. Template nội bộ ghi RA/AI thì cùng gói này |
| Allocation | Gán một nhu cầu vào đúng component. Enumerate và uevent gán cho USB-driver. Session Android Auto và MFi không gán cho USB-driver |
| Derived requirement | Yêu cầu sinh ra từ thiết kế hoặc từ ràng buộc kỹ thuật, không chép nguyên câu khách hàng. REQ-016 (HCD được phép khác giữa các Pi, class và sysfs giữ nguyên) là dạng derived |

### SWE.1 đến SWE.6

| Thuật ngữ | Nghĩa trong task này |
|---|---|
| SWE.1 Software Requirements Analysis | Chốt yêu cầu phần mềm, review, trace lên feature. Đầu ra là SRS |
| SRS | Software Requirements Specification. Với module này là bảng `USBDRV-REQ-xxx` sau review, mỗi câu quan sát được khi test |
| SWE.2 Software Architectural Design | Chia component thành phần tử và giao diện. Ở đây là HCD, usbcore, class driver, cộng hợp đồng với USB-Manager. Phần Linux đánh dấu `reused` |
| SWE.3 Software Detailed Design and Unit Construction | Thiết kế chi tiết và dựng đơn vị. Chỉ phần mình viết: fragment config, DTS, patch. Source upstream được tham chiếu kèm version, không chép vào design |
| Unit | Đơn vị xây dựng nhỏ nhất đem đi kiểm SWE.4. Đơn vị owned là file cfg hoặc một patch. Cả `hub.c` không phải unit của công ty |
| SWE.4 Software Unit Verification | Kiểm từng unit owned. Với file config, kiểm bằng inspection (`/proc/config.gz` khớp SRS). Unit upstream có justification thay cho unit test |
| SWE.5 Software Integration and Integration Test | Ghép HCD, usbcore, class driver trên một board và kiểm chúng chạy cùng nhau. Chưa cần chứng minh hết SRS. Quan sát bằng `lsusb`, udev, sysfs |
| SWE.6 Software Qualification Test | Kiểm cả component đối chiếu baseline SRS. Trong ASPICE 3.1 đây là qualification: pass khi REQ `Standard` và `Owned` có bằng chứng, REQ `Open` không bị tính là đã xong |
| Qualification | Kết luận “yêu cầu phần mềm của component đã được kiểm trên môi trường đã khai”. Câu ghi trên biên bản lab là qualified trên Raspberry Pi, Meter-EVB còn pending |
| Verification | Kiểm sản phẩm có đúng yêu cầu đã viết hay không. SWE.4, SWE.5, SWE.6 đều là verification ở các mức khác nhau |
| Validation | Kiểm sản phẩm có đúng nhu cầu người dùng trên xe hay không. Việc đó ở mức system, không thay bằng SWE.6 của riêng USB-driver |

### Từ dùng khi ghi yêu cầu và test

| Thuật ngữ | Nghĩa trong task này |
|---|---|
| `Standard` | Linux upstream thực hiện. Mình test ở biên, không viết design chi tiết |
| `Owned` | Công ty thực hiện: chọn config, DTS, quyết định Host trên cổng feature |
| `Open` | Chưa gán xong vì thiếu schematic EVB hoặc chưa có quyết định PM. Không đưa vào danh sách pass |
| Reused | Phần tử lấy từ phần mềm có sẵn và pin version. usbcore và `cdc_ncm` là reused. Đổi version là một lần IA mới |
| Justification | Giải thích đã review vì sao một BP không làm theo lối thông thường. Ví dụ không unit-test `cdc_ncm.c` vì đó là reused, kiểm bằng SWE.5/SWE.6 |
| Inspection | Kiểm bằng đọc và đối chiếu, có ghi nhận, không chỉ chạy máy. SWE.4 của fragment cfg là inspection |
| Smoke test | Nhóm case ngắn sau khi đổi kernel: REQ-001, 002, 008, 011, 016. Đủ để thấy stack còn sống, không thay bộ SWE.6 đầy đủ |
| Waiver | Cho phép một case không chạy, có lý do và người review. TC-OC-001 trên Pi là waiver nếu cổng không tạo được quá dòng. Waiver không biến case thành pass |
| Blocked | Case chưa chạy vì thiếu đồ (cáp, Pi, phone). Ghi lý do. Khác waiver: blocked là nợ, sẽ chạy khi đủ đồ |
| Entry / exit (tiêu chí thoát) | Điều kiện để được coi một process đã xong trong kế hoạch này. Ví dụ SWE.1 thoát khi review hết comment major và USB-Manager đã nhận các REQ về uevent |
