#ifndef CM_USB_DBUS_API_H
#define CM_USB_DBUS_API_H

/*
 * Lớp D-Bus của connectivity manager cho phần USB.
 * Duy nhất file này là C++ (src/dbus_sdbuspp.cpp) vì sdbus-c++ không có API C.
 * Service Android Auto, CarPlay và UI là client.
 *
 * Service:   org.example.connectivity
 * Path:      /org/example/connectivity/usb
 * Interface: org.example.connectivity.Usb1
 *
 * Method:  ListDevices, StartSession, StopSession
 * Signal:  DeviceChanged, SessionChanged, ProjectionAvailable
 * Property: ActiveSession
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "manager.h"

typedef struct usb_dbus usb_dbus_t;

usb_dbus_t* usb_dbus_start(usb_registry_t* registry, usb_manager_t* manager);
void usb_dbus_stop(usb_dbus_t* bus);
void usb_dbus_emit_device(usb_dbus_t* bus, const usb_device_t* device);
void usb_dbus_emit_session(usb_dbus_t* bus, const usb_session_t* session);
void usb_dbus_emit_projection(usb_dbus_t* bus, const char* device_id, const char* mode);

#ifdef __cplusplus
}
#endif

#endif
