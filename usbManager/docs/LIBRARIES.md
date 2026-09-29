# Thư viện dùng trong usb-manager

Cả hai cây `usb-manager/` (C++) và `usb-manager-c/` (C) dùng cùng các thư viện hệ thống. D-Bus là chỗ khác nhau: C++ gọi **sd-bus** trong libsystemd, cây C bọc **sdbus-c++** rồi xuất `extern "C"`.

Cột dưới đây là function module này **đang gọi** hoặc **cần gọi tiếp** khi chuyển từ probe sang session thật. Không liệt kê hết API của từng thư viện.

## Ánh xạ C++ ↔ C

| C++ | C | Thư viện |
|-----|---|----------|
| `UdevMonitor` | `usb_udev_monitor_*` | libudev |
| `TransportProber` | `usb_probe_aoap` / `usb_probe_ncm` | libusb-1.0, sysfs |
| `UsbDbusService` | `usb_dbus_*` trong `dbus_sdbuspp.cpp` | sd-bus hoặc sdbus-c++ |
| `DeviceRegistry` | `usb_registry_*` | pthread (chỉ cây C khóa tường minh) |

## libudev

Hotplug USB host. Cả hai nguồn subscribe netlink `udev`, lọc `usb` / `usb_device`, rồi đọc VID/PID và serial.

| Function | Việc cần làm |
|----------|----------------|
| `udev_new` / `udev_unref` | Tạo và thả context. Gọi một lần khi daemon start/stop. |
| `udev_monitor_new_from_netlink(ctx, "udev")` | Lắng nghe sự kiện đã qua udev (không dùng `"kernel"`). |
| `udev_monitor_filter_add_match_subsystem_devtype(..., "usb", "usb_device")` | Chỉ nhận thiết bị USB, bỏ qua interface con. |
| `udev_monitor_enable_receiving` | Bắt đầu nhận sau khi đã gắn filter. |
| `udev_monitor_get_fd` | fd đưa vào `poll` / `select`. |
| `udev_monitor_receive_device` | Đọc một sự kiện. Luôn `udev_device_unref` sau khi đã copy dữ liệu. |
| `udev_device_get_action` | `"add"` hoặc `"remove"`. |
| `udev_device_get_syspath` / `get_devnode` | sysfs path và `/dev/bus/usb/...`. |
| `udev_device_get_property_value` | `ID_VENDOR_ID`, `ID_MODEL_ID`, `ID_SERIAL_SHORT`, `ID_USB_INTERFACES`. |
| `udev_device_get_sysattr_value` | Dự phòng khi property trống: `idVendor`, `idProduct`, `manufacturer`, `product`, `serial`, `busnum`, `devnum`. |
| `udev_enumerate_new` + `add_match_subsystem` + `scan_devices` | Quét thiết bị đã cắm lúc daemon khởi động (`enumerateExisting`). |
| `udev_device_new_from_syspath` | Mở từng device trong danh sách enumerate. |

C++: `usb-manager/src/hotplug/UdevMonitor.cpp`.  
C: `usb-manager-c/src/udev_monitor.c`.

Daemon chạy root trên image Yocto nên mở được node USB. Trên máy dev, thiếu quyền thì libusb báo `open_failed` dù udev vẫn thấy thiết bị.

## libusb-1.0

Chỉ dùng cho bước probe AOAP (Android Open Accessory), không dùng để claim interface hay đọc bulk. Cùng chuỗi nằm ở `TransportProber::probeAoap` và `usb_probe_aoap`.

| Function | Việc cần làm |
|----------|----------------|
| `libusb_init` / `libusb_exit` | Context tạm cho một lần probe. Hiện tại tạo mới mỗi lần gọi. |
| `libusb_open_device_with_vid_pid` | Mở handle theo VID/PID. `NULL` nghĩa là quyền hoặc thiết bị đang bận. |
| `libusb_control_transfer` | `bmRequestType = 0xC0` (device-to-host, vendor, device), `bRequest = 51` (`GET_PROTOCOL`), `wLength = 2`, timeout 1000 ms. Protocol `>= 1` thì máy hỗ trợ AOAP. |
| `libusb_close` | Đóng handle trước `libusb_exit`. |

Hằng đã ghi trong source nhưng **chưa gọi**:

| Request | Hướng | Khi nào cần |
|---------|--------|-------------|
| 51 `GET_PROTOCOL` | IN | Đã dùng. |
| 52 `SEND_STRING` | OUT | Gửi manufacturer, model, description, version, URI, serial trước khi vào accessory mode. |
| 53 `START` | OUT | Báo điện thoại chuyển sang mode AOAP. Sau đó máy re-enumerate, VID Google `0x18d1`, PID `0x2d00`–`0x2d05`. |

PID trong khoảng đó được coi là `already_in_aoap_mode` và không gửi `GET_PROTOCOL` nữa.

`probeNcm` không gọi libusb. Nó đọc `/sys/class/net` để tìm interface (`usb0`, `eth*`) trỏ vào sysfs của cùng thiết bị USB — dùng cho CDC NCM / RNDIS sau khi phone đổi mode.

Header: `libusb-1.0/libusb.h`. Package Yocto: `libusb1`.

## sd-bus (C++)

`UsbDbusService` dùng sd-bus của systemd, không dùng libdbus trực tiếp.

Well-known name: `org.example.connectivity`  
Path: `/org/example/connectivity/usb`  
Interface: `org.example.connectivity.Usb1`

| Function / macro | Việc cần làm |
|------------------|--------------|
| `sd_bus_open_user` rồi `sd_bus_open_system` | Máy dev thử session bus trước. Trên board, system bus là bus chạy được. |
| `SD_BUS_VTABLE_START` / `SD_BUS_METHOD` / `SD_BUS_SIGNAL` / `SD_BUS_PROPERTY` / `SD_BUS_VTABLE_END` | Khai báo `ListDevices`, `StartSession(ss)→b`, `StopSession(s)→b`, signal `DeviceChanged`/`SessionChanged` (`sss`), property `ActiveSession`. |
| `sd_bus_add_object_vtable` | Gắn vtable vào path. Userdata trỏ `DeviceRegistry` và `SessionOrchestrator`. |
| `sd_bus_request_name` | Xin well-known name. Object vẫn gọi được bằng unique name nếu name này bị chiếm. |
| `sd_bus_process` + `sd_bus_wait` | Vòng xử lý chạy trên thread riêng. `sd_bus_wait` timeout 200 ms để `stop()` thoát được. |
| `sd_bus_message_read` | Đọc tham số method (`"ss"` hoặc `"s"`). |
| `sd_bus_reply_method_return` | Trả `b` (boolean). |
| `sd_bus_error_set_const` / `sd_bus_error_setf` | `org.example.InvalidMode`, `StartFailed`, `StopFailed`. |
| `sd_bus_message_open_container` / `append` / `close_container` | `ListDevices` trả `aa{sv}`: mỗi device là dictionary string. |
| `sd_bus_emit_signal` | `DeviceChanged(device_id, state, type)` và `SessionChanged(device_id, state, reason)`. |
| `sd_bus_slot_unref` / `sd_bus_unref` | Trong `stop()`. |

File: `usb-manager/src/ipc/UsbDbusService.cpp`.

## sdbus-c++ (cây C)

`libdbus-cpp` trong yêu cầu là thư viện này: header `<sdbus-c++/sdbus-c++.h>`, namespace `sdbus`. Chỉ một file C++ trong cây C: `usb-manager-c/src/dbus_sdbuspp.cpp`. Phần còn lại gọi API C `usb_dbus_start`, `usb_dbus_stop`, `usb_dbus_emit_device`, `usb_dbus_emit_session`.

| API sdbus-c++ | Tương đương sd-bus |
|---------------|--------------------|
| `sdbus::createSessionBusConnection` / `createSystemBusConnection` | `sd_bus_open_user` / `sd_bus_open_system` |
| `sdbus::createObject` | object tại cùng path |
| `registerMethod(...).implementedAs(...)` | `SD_BUS_METHOD` |
| `registerSignal(...).withParameters<string,string,string>()` | `SD_BUS_SIGNAL "sss"` |
| `registerProperty(...).withGetter(...)` | `SD_BUS_PROPERTY` |
| `finishRegistration` | xong vtable |
| `enterEventLoopAsync` | thread `sd_bus_process` / `sd_bus_wait` |
| `emitSignal(...).withArguments(...)` | `sd_bus_emit_signal` |
| `sdbus::Error` | `sd_bus_error`, ném khi mode không hợp lệ |

Cùng service, path và interface với bản C++. `ListDevices` trả `vector<map<string, Variant>>`, cùng các key `device_id`, `type`, `state`, `vid`, `pid`, `aoap`, `ncm`.

## pthread

Cây C khóa registry, orchestrator và log bằng `pthread_mutex`. C++ dùng `std::mutex` / `std::thread` (thread D-Bus) nên không include `<pthread.h>` trực tiếp. Không gọi API thread nào khác.

Khi thêm work USB blocking trong adapter, giữ probe và control transfer ngắn; không giữ mutex của registry trong lúc `libusb_control_transfer`.

## Việc chưa thuộc các thư viện này

- `DemoAasdkSession` chỉ log kênh Video, Audio, Input, Sensor. Chưa link AASDK hay OpenAuto. Khi thay, giữ biên `IAasdkSession::{start,stop,isActive}`.
- `CarPlayStubAdapter::start` luôn thất bại với `MFI_REQUIRED`. Không có thư viện MFi/IAP trong tree.
- NCM chỉ được phát hiện qua sysfs, chưa mở socket trên `netInterface`.
