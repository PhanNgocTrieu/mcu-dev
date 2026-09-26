#ifndef USB_MGR_CLASSIFIER_H
#define USB_MGR_CLASSIFIER_H

#include "types.h"

usb_device_type_t usb_classify(const usb_device_t* info);
void usb_classify_enrich(usb_device_t* info);

#endif
