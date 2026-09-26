#ifndef USB_MGR_TRANSPORT_H
#define USB_MGR_TRANSPORT_H

#include "types.h"

typedef struct {
  int attempted;
  int supported;
  char detail[USB_ERR_LEN];
} usb_aoap_result_t;

typedef struct {
  int iface_present;
  char if_name[USB_IFACE_LEN];
  char detail[USB_ERR_LEN];
} usb_ncm_result_t;

void usb_probe_aoap(const usb_device_t* device, usb_aoap_result_t* out);
void usb_probe_ncm(const usb_device_t* device, usb_ncm_result_t* out);

#endif
