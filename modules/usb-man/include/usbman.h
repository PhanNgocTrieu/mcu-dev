/**
 * @file usbman.h
 * @brief USB manager session policy (one projection backend at a time).
 *
 * Android goes through AOA then libhu-aa (AASDK on board images).
 * Apple CarPlay requires CDC-NCM + IPv6 (never ipheth) then libhu-carplay.
 * Media codecs and IAP2/MFi live in those libraries, not here.
 */
#ifndef USBMAN_H
#define USBMAN_H

#include <stddef.h>

#define USBMAN_ID_LEN 80
#define USBMAN_STR_LEN 96
#define USBMAN_LINE_LEN 256

typedef enum {
    USBMAN_KIND_OTHER = 0,
    USBMAN_KIND_ANDROID,
    USBMAN_KIND_ANDROID_AOAP,
    USBMAN_KIND_CARPLAY,
    USBMAN_KIND_APPLE_WAIT,
    USBMAN_KIND_APPLE_IPHETH,
    USBMAN_KIND_STORAGE,
    USBMAN_KIND_HID
} usbman_kind_t;

typedef enum {
    USBMAN_ACT_NONE = 0,
    USBMAN_ACT_AOA,
    USBMAN_ACT_LINK_UP
} usbman_action_t;

typedef struct {
    char id[USBMAN_ID_LEN];
    char serial[USBMAN_STR_LEN];
    char mfg[USBMAN_STR_LEN];
    char prod[USBMAN_STR_LEN];
    char ifaces[160];
    char drivers[96];
    char net[32];
    char node[96];
    unsigned vid;
    unsigned pid;
    int ncm, ecm, rndis, ipheth, storage, hid, adb, accessory, sim, apple;
} usbman_dev_t;

typedef struct {
    int x;
    int y;
    int down;
} usbman_touch_t;

typedef struct {
    char backend[16];
    char phase[24];
    char device[USBMAN_ID_LEN];
    char reason[USBMAN_STR_LEN];
    char parked[USBMAN_ID_LEN];
    char net[32];
    int streaming;
    int stream_user;
    int pending_reenum;
    int touch_count;
    usbman_touch_t last_touch;
    char other[USBMAN_ID_LEN];
} usbman_session_t;

const char *usbman_kind_str(usbman_kind_t kind);
usbman_kind_t usbman_classify(const usbman_dev_t *dev);

void usbman_session_init(usbman_session_t *s);
usbman_action_t usbman_on_device(usbman_session_t *s, const usbman_dev_t *dev);
void usbman_on_gone(usbman_session_t *s, const char *id);
void usbman_expect_reenum(usbman_session_t *s);
void usbman_fail(usbman_session_t *s, const char *reason);
void usbman_touch(usbman_session_t *s, int x, int y, int down);
int usbman_set_stream(usbman_session_t *s, int on);
void usbman_state_line(const usbman_session_t *s, char *out, size_t n);

/** @brief Build claim + GET_PROTOCOL + six strings + START. */
int usbman_aoa_build(const char *id, char lines[][USBMAN_LINE_LEN], int cap);

int usbman_parse_dev(const char *line, usbman_dev_t *dev);

#endif
