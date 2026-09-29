#ifndef CM_USB_CLASSIFIER_H
#define CM_USB_CLASSIFIER_H

#include "types.h"

/*
 * Phân loại theo descriptor kernel đã đọc. Không viết driver.
 *
 * Thứ tự: iPhone (VID Apple) -> Android (VID đã biết, MTP, ADB, AOAP)
 * -> mass storage (class 08) -> HID (class 03) -> unknown.
 * Interface vendor-specific (class FF) một mình không phải Android.
 */
usb_device_type_t usb_classify(const usb_device_t* info);
void usb_classify_enrich(usb_device_t* info);

#endif
