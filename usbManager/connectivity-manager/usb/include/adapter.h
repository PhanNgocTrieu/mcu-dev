#ifndef CM_USB_ADAPTER_H
#define CM_USB_ADAPTER_H

#include "types.h"

/*
 * Adapter là điểm móc khi service Android Auto hoặc CarPlay gọi StartSession.
 * Protocol thật nằm ở service đó. Ở đây start/stop chỉ giữ phiên exclusive
 * trên cổng USB. CarPlay prototype trả MFI_REQUIRED vì chưa có chip MFi.
 */

typedef struct {
    const char* name;
    int (*probe)(usb_device_t* device, char* msg, size_t msg_len, void* ctx);
    int (*start)(const usb_device_t* device, char* msg, size_t msg_len, void* ctx);
    int (*stop)(const usb_device_t* device, void* ctx);
    void* ctx;
} usb_adapter_t;

void usb_adapter_android_init(usb_adapter_t* adapter);
void usb_adapter_carplay_init(usb_adapter_t* adapter);
void usb_adapter_shutdown(usb_adapter_t* adapter);

#endif
