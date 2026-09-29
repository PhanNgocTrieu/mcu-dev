#ifndef USB_MGR_LOG_H
#define USB_MGR_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { USB_LOG_DEBUG = 0, USB_LOG_INFO, USB_LOG_WARN, USB_LOG_ERROR } usb_log_level_t;

void usb_log_set_level(usb_log_level_t level);
void usb_log_write(usb_log_level_t level, const char* file, int line, const char* msg);

#define USB_LOG_DEBUG(msg) usb_log_write(USB_LOG_DEBUG, __FILE__, __LINE__, msg)
#define USB_LOG_INFO(msg) usb_log_write(USB_LOG_INFO, __FILE__, __LINE__, msg)
#define USB_LOG_WARN(msg) usb_log_write(USB_LOG_WARN, __FILE__, __LINE__, msg)
#define USB_LOG_ERROR(msg) usb_log_write(USB_LOG_ERROR, __FILE__, __LINE__, msg)

#ifdef __cplusplus
}
#endif

#endif
