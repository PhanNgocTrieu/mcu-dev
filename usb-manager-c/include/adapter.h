#ifndef USB_MGR_ADAPTER_H
#define USB_MGR_ADAPTER_H

#include "types.h"

typedef struct {
  const char* name;
  int (*probe)(usb_device_t* device, char* msg, size_t msg_len, void* ctx);
  int (*start)(const usb_device_t* device, char* msg, size_t msg_len, void* ctx);
  int (*stop)(const usb_device_t* device, void* ctx);
  void* ctx;
} usb_adapter_t;

void usb_adapter_android_init(usb_adapter_t* adapter);
void usb_adapter_carplay_init(usb_adapter_t* adapter);

#endif
