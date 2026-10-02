#ifndef USBMOD_H
#define USBMOD_H

/*
 * USB Module — phiên USB phía trên USB Driver.
 *
 * Enumeration lấy từ usbdrv (sysfs / uevent). AOA là control transfer trên
 * usbfs. AirPlay chỉ tới mức transport: máy Apple đã có netdev (NCM, ECM,
 * RNDIS, ipheth). Protocol media AirPlay không nằm trong module này.
 */

#include "usbdrv.h"

#include <stddef.h>
#include <stdint.h>

#define USBMOD_ID_LEN 160
#define USBMOD_ERR_LEN 192

typedef enum {
    USBMOD_KIND_UNKNOWN = 0,
    USBMOD_KIND_ANDROID,
    USBMOD_KIND_APPLE,
    USBMOD_KIND_STORAGE,
    USBMOD_KIND_HID,
    USBMOD_KIND_NETWORK
} usbmod_kind_t;

typedef enum {
    USBMOD_STATE_ENUMERATING = 0,
    USBMOD_STATE_CLASSIFIED,
    USBMOD_STATE_PROBING,
    USBMOD_STATE_READY,
    USBMOD_STATE_ACTIVE,
    USBMOD_STATE_FAILED,
    USBMOD_STATE_IGNORED
} usbmod_state_t;

typedef enum {
    USBMOD_MODE_NONE = 0,
    USBMOD_MODE_AOA,
    USBMOD_MODE_AIRPLAY,
    USBMOD_MODE_STORAGE
} usbmod_mode_t;

typedef struct {
    char device_id[USBMOD_ID_LEN];
    char sys_name[USBDRV_NAME_LEN];
    uint16_t vendor_id;
    uint16_t product_id;
    char serial[USBDRV_STR_LEN];
    usbmod_kind_t kind;
    usbmod_state_t state;
    usbmod_mode_t mode;
    int aoa_protocol;
    int airplay_ready;
    char net_iface[USBDRV_NAME_LEN];
    char devnode[USBDRV_PATH_LEN];
    char block_dev[USBDRV_NAME_LEN];
    char reason[USBMOD_ERR_LEN];
} usbmod_device_t;

typedef struct {
    const char *manufacturer;
    const char *model;
    const char *description;
    const char *version;
    const char *uri;
    const char *serial;
} usbmod_aoa_strings_t;

#define USBMOD_AOA_STEPS 8

typedef struct {
    usbdrv_control_t setup;
    char payload[USBDRV_STR_LEN];
    size_t payload_len;
    int data_in;
} usbmod_aoa_step_t;

typedef struct {
    usbmod_aoa_step_t steps[USBMOD_AOA_STEPS];
    size_t count;
} usbmod_aoa_plan_t;

typedef struct usbmod usbmod_t;

typedef struct {
    void (*on_device)(const usbmod_device_t *device, const char *event, void *user);
    void *user;
} usbmod_listener_t;

const char *usbmod_kind_str(usbmod_kind_t kind);
const char *usbmod_state_str(usbmod_state_t state);
const char *usbmod_mode_str(usbmod_mode_t mode);
int usbmod_mode_from_str(const char *text, usbmod_mode_t *out);

usbmod_kind_t usbmod_classify(const usbdrv_device_t *device);
int usbmod_airplay_ready(const usbdrv_device_t *device);

int usbmod_aoa_build(const usbmod_aoa_strings_t *strings, usbmod_aoa_plan_t *plan);
int usbmod_aoa_run(const char *devnode, const usbmod_aoa_strings_t *strings, int *protocol_out,
                   char *error, size_t error_len);

usbmod_t *usbmod_create(void);
void usbmod_destroy(usbmod_t *mod);
void usbmod_set_listener(usbmod_t *mod, const usbmod_listener_t *listener);
void usbmod_set_switch_aoa(usbmod_t *mod, int enabled);
void usbmod_set_aoa_strings(usbmod_t *mod, const usbmod_aoa_strings_t *strings);

int usbmod_handle_add(usbmod_t *mod, const usbdrv_device_t *device);
int usbmod_handle_remove(usbmod_t *mod, const char *sys_name);
int usbmod_scan(usbmod_t *mod, const char *sysfs_devices);
int usbmod_finish_probe(usbmod_t *mod, const char *device_id, int protocol);

int usbmod_start(usbmod_t *mod, const char *device_id, usbmod_mode_t mode, char *error, size_t error_len);
int usbmod_stop(usbmod_t *mod, const char *device_id, char *error, size_t error_len);

size_t usbmod_count(const usbmod_t *mod);
int usbmod_get(const usbmod_t *mod, size_t index, usbmod_device_t *out);
int usbmod_find(const usbmod_t *mod, const char *device_id, usbmod_device_t *out);

#endif
