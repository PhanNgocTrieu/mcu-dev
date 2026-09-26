#ifndef USB_MGR_DBUS_API_H
#define USB_MGR_DBUS_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "orchestrator.h"

/*
 * C API over the sdbus-c++ binding.
 * Implementation lives in src/dbus_sdbuspp.cpp (the only C++ unit).
 */
typedef struct usb_dbus usb_dbus_t;

usb_dbus_t* usb_dbus_start(usb_registry_t* registry, usb_orchestrator_t* orch);
void usb_dbus_stop(usb_dbus_t* bus);
void usb_dbus_emit_device(usb_dbus_t* bus, const usb_device_t* device);
void usb_dbus_emit_session(usb_dbus_t* bus, const usb_session_t* session);

#ifdef __cplusplus
}
#endif

#endif
