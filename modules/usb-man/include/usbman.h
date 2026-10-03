/**
 * @file usbman.h
 * @brief Chính sách session USB manager (một backend projection tại một thời điểm).
 *
 * Android: AOA → libhu-aa (AASDK trên image board).
 * Apple CarPlay: bắt buộc CDC-NCM (không dùng ipheth) → libhu-carplay.
 * Codec media / IAP2 / MFi nằm trong các thư viện đó, không ở đây.
 */
#ifndef USBMAN_H
#define USBMAN_H

#include <stddef.h>

#define USBMAN_ID_LEN 80
#define USBMAN_STR_LEN 96
#define USBMAN_LINE_LEN 256

/** Kết quả phân loại thiết bị cho policy. */
typedef enum {
    USBMAN_KIND_OTHER = 0,
    USBMAN_KIND_ANDROID,       /* Google/ADB — cần AOA */
    USBMAN_KIND_ANDROID_AOAP,  /* đã vào accessory mode */
    USBMAN_KIND_CARPLAY,       /* Apple + CDC-NCM */
    USBMAN_KIND_APPLE_WAIT,    /* Apple, chưa có NCM */
    USBMAN_KIND_APPLE_IPHETH,  /* Apple ipheth — bị từ chối */
    USBMAN_KIND_STORAGE,
    USBMAN_KIND_HID
} usbman_kind_t;

/** Hành động daemon cần làm sau usbman_on_device(). */
typedef enum {
    USBMAN_ACT_NONE = 0,
    USBMAN_ACT_AOA,      /* xếp hàng lệnh AOA xuống driver */
    USBMAN_ACT_LINK_UP   /* `ip link set <net> up` cho NCM */
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

/** Trạng thái session hiện tại — serialize thành dòng `state ...`. */
typedef struct {
    char backend[16];          /* none | android | carplay */
    char phase[24];            /* idle|aoa|reenumerating|active|failed|classified */
    char device[USBMAN_ID_LEN];
    char reason[USBMAN_STR_LEN];
    char parked[USBMAN_ID_LEN]; /* máy projection bị giữ vì backend đang bận */
    char net[32];
    int streaming;             /* có đẩy video không (sau khi kết hợp stream_user) */
    int stream_user;           /* -1 auto, 0 tắt, 1 bật */
    int pending_reenum;        /* 1 = vừa gửi AOA START, chờ phone plug lại */
    int touch_count;
    usbman_touch_t last_touch;
    char other[USBMAN_ID_LEN]; /* thiết bị không-projection gần nhất */
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
