#ifndef CM_USB_TYPES_H
#define CM_USB_TYPES_H

/*
 * Kiểu dùng chung của USB manager.
 *
 * USB manager là một phần của connectivity manager: nó sở hữu cổng USB Host,
 * phân loại thiết bị, giữ state machine, mount mass storage, và báo cho
 * service Android Auto / CarPlay khi thiết bị sẵn sàng. Hai service đó tự
 * gọi StartSession. UI chỉ nghe signal, không khởi động phiên.
 */

#include <stddef.h>
#include <stdint.h>

#define USB_ID_LEN 160
#define USB_STR_LEN 128
#define USB_PATH_LEN 512
#define USB_IFACE_LEN 32
#define USB_ERR_LEN 192

typedef enum {
    USB_TYPE_UNKNOWN = 0,
    USB_TYPE_ANDROID,
    USB_TYPE_IPHONE,
    USB_TYPE_MASS_STORAGE,
    USB_TYPE_HID
} usb_device_type_t;

/*
 * Vòng đời thiết bị. Session Android Auto / CarPlay là máy trạng thái riêng
 * (usb_session_state_t): device về Ready khi service dừng phiên, kể cả khi
 * phiên vừa thất bại, để service kia có thể thử lại khi điện thoại còn cắm.
 *
 *   Enumerating -> Classified -> Probing -> Ready
 *                      |             |
 *                      v             v
 *                   Ignored        Failed
 *   Ready/Failed/Ignored -> Disconnecting -> (xóa khỏi registry)
 */
typedef enum {
    USB_STATE_IDLE = 0,
    USB_STATE_ENUMERATING,
    USB_STATE_CLASSIFIED,
    USB_STATE_PROBING,
    USB_STATE_READY,
    USB_STATE_CONNECTING,
    USB_STATE_ACTIVE,
    USB_STATE_FAILED,
    USB_STATE_DISCONNECTING,
    USB_STATE_IGNORED
} usb_device_state_t;

typedef enum {
    USB_MODE_NONE = 0,
    USB_MODE_ANDROID_AUTO,
    USB_MODE_CARPLAY,
    USB_MODE_STORAGE
} usb_session_mode_t;

typedef enum {
    USB_SESSION_IDLE = 0,
    USB_SESSION_STARTING,
    USB_SESSION_ACTIVE,
    USB_SESSION_STOPPING,
    USB_SESSION_FAILED
} usb_session_state_t;

typedef struct {
    /* Ưu tiên serial. Không dùng busnum-devnum: kernel tái sử dụng sau khi rút. */
    char device_id[USB_ID_LEN];
    char sys_path[USB_PATH_LEN];
    char dev_node[USB_PATH_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
    char manufacturer[USB_STR_LEN];
    char product[USB_STR_LEN];
    char serial[USB_STR_LEN];
    char interfaces[USB_STR_LEN];
    usb_device_type_t type;
    usb_device_state_t state;
    char last_error[USB_ERR_LEN];
    int aoap_supported;
    int ncm_present;
    char net_iface[USB_IFACE_LEN];
    /* Mass storage. Rỗng nếu không phải ổ đĩa hoặc mount thất bại. */
    char block_dev[USB_PATH_LEN];
    char mount_point[USB_PATH_LEN];
} usb_device_t;

typedef struct {
    char device_id[USB_ID_LEN];
    usb_session_mode_t mode;
    usb_session_state_t state;
    char reason[USB_ERR_LEN];
} usb_session_t;

const char* usb_type_str(usb_device_type_t type);
const char* usb_state_str(usb_device_state_t state);
const char* usb_mode_str(usb_session_mode_t mode);
const char* usb_session_state_str(usb_session_state_t state);
int usb_mode_from_str(const char* text, usb_session_mode_t* out);

/* serial -> usb-VID-PID-serial; không có serial thì dùng cổng vật lý + VID/PID. */
void usb_assign_id(usb_device_t* device);

#endif
