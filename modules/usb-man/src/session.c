/**
 * @file session.c
 * @brief Máy trạng thái session projection (một backend tại một thời điểm).
 *
 * Phase chính:
 *   idle → aoa → reenumerating → active   (Android: ADB → AOA START → AOAP)
 *   idle → classified/active              (Apple: chờ NCM → CarPlay)
 *   * → failed                            (AOA lỗi, ipheth bị chặn, ...)
 *
 * stream_user: -1 = mặc định (auto khi active), 0 = user tắt, 1 = user bật.
 */
#include "usbman.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>

/* Thiết bị đủ điều kiện tranh session projection (không phải storage/HID thường). */
static int projection(usbman_kind_t kind)
{
    return kind == USBMAN_KIND_ANDROID || kind == USBMAN_KIND_ANDROID_AOAP ||
           kind == USBMAN_KIND_CARPLAY || kind == USBMAN_KIND_APPLE_WAIT ||
           kind == USBMAN_KIND_APPLE_IPHETH;
}

/* Reset về trạng thái không có projection; stream_user về auto (-1). */
static void set_idle(usbman_session_t *s)
{
    snprintf(s->backend, sizeof s->backend, "none");
    snprintf(s->phase, sizeof s->phase, "idle");
    s->device[0] = '\0';
    s->net[0] = '\0';
    snprintf(s->reason, sizeof s->reason, "-");
    s->streaming = 0;
    s->pending_reenum = 0;
    s->stream_user = -1;
}

/*
 * Có đẩy video không?
 *  - chỉ khi phase == active
 *  - stream_user == 0 → user tắt tường minh
 *  - stream_user == -1 hoặc 1 → cho phép (auto / bật)
 */
static int streaming_now(const usbman_session_t *s)
{
    if (strcmp(s->phase, "active") != 0) {
        return 0;
    }
    if (s->stream_user == 0) {
        return 0;
    }
    return 1;
}

static void refresh_stream(usbman_session_t *s)
{
    s->streaming = streaming_now(s);
}

void usbman_session_init(usbman_session_t *s)
{
    memset(s, 0, sizeof *s);
    s->stream_user = -1; /* auto: stream khi active */
    snprintf(s->parked, sizeof s->parked, "-");
    snprintf(s->other, sizeof s->other, "-");
    set_idle(s);
}

/* Gán backend/phase/device/net; nếu máy này từng parked thì xóa parked. */
static void adopt(usbman_session_t *s, const usbman_dev_t *dev, const char *backend, const char *phase,
                  const char *reason)
{
    snprintf(s->backend, sizeof s->backend, "%s", backend);
    snprintf(s->phase, sizeof s->phase, "%s", phase);
    snprintf(s->device, sizeof s->device, "%s", dev->id);
    snprintf(s->net, sizeof s->net, "%s", dev->net[0] ? dev->net : "-");
    snprintf(s->reason, sizeof s->reason, "%s", reason);
    if (strcmp(s->parked, dev->id) == 0) {
        snprintf(s->parked, sizeof s->parked, "-");
    }
    refresh_stream(s);
}

/**
 * Thiết bị xuất hiện/đổi: cập nhật session và trả action cho daemon thực thi.
 * @return USBMAN_ACT_AOA (chạy AOA), USBMAN_ACT_LINK_UP (ip link set up), hoặc NONE.
 */
usbman_action_t usbman_on_device(usbman_session_t *s, const usbman_dev_t *dev)
{
    usbman_kind_t kind = usbman_classify(dev);
    int same;

    if (!projection(kind)) {
        /* Lưu id thiết bị "khác" để UI/debug; không chiếm backend. */
        snprintf(s->other, sizeof s->other, "%s", dev->id);
        return USBMAN_ACT_NONE;
    }

    same = s->device[0] && strcmp(s->device, dev->id) == 0;
    /* Đã có session đang chạy → park máy mới, không preempt. */
    if (!same && strcmp(s->backend, "none") != 0 && strcmp(s->phase, "idle") != 0 &&
        strcmp(s->phase, "failed") != 0) {
        snprintf(s->parked, sizeof s->parked, "%s", dev->id);
        snprintf(s->reason, sizeof s->reason, "backend-busy");
        return USBMAN_ACT_NONE;
    }

    if (kind == USBMAN_KIND_ANDROID_AOAP) {
        /* Phone đã vào accessory mode → sẵn sàng media AA. */
        adopt(s, dev, "android", "active", "aoap");
        s->pending_reenum = 0;
        refresh_stream(s);
        return USBMAN_ACT_NONE;
    }
    if (kind == USBMAN_KIND_ANDROID) {
        if (same && strcmp(s->phase, "aoa") == 0) {
            return USBMAN_ACT_NONE; /* đang chạy AOA, tránh lặp */
        }
        adopt(s, dev, "android", "aoa", "aoa-switch");
        return USBMAN_ACT_AOA;
    }
    if (kind == USBMAN_KIND_CARPLAY) {
        if (same && strcmp(s->phase, "active") == 0) {
            /* Cập nhật tên iface nếu kernel đổi; sim không cần `ip link`. */
            snprintf(s->net, sizeof s->net, "%s", dev->net[0] ? dev->net : "-");
            return dev->sim ? USBMAN_ACT_NONE : USBMAN_ACT_LINK_UP;
        }
        adopt(s, dev, "carplay", "active", "ncm");
        return dev->sim ? USBMAN_ACT_NONE : USBMAN_ACT_LINK_UP;
    }
    if (kind == USBMAN_KIND_APPLE_WAIT) {
        /* Apple đã thấy nhưng chưa có CDC-NCM — chờ kernel/driver. */
        adopt(s, dev, "carplay", "classified", "waiting-ncm");
        return USBMAN_ACT_NONE;
    }
    if (kind == USBMAN_KIND_APPLE_IPHETH) {
        /* Chính sách HUPI: không dùng ipheth; bắt buộc NCM. */
        adopt(s, dev, "carplay", "failed", "ipheth-disabled");
        return USBMAN_ACT_NONE;
    }
    return USBMAN_ACT_NONE;
}

void usbman_on_gone(usbman_session_t *s, const char *id)
{
    if (!id) {
        return;
    }
    if (strcmp(s->parked, id) == 0) {
        snprintf(s->parked, sizeof s->parked, "-");
    }
    if (strcmp(s->other, id) == 0) {
        snprintf(s->other, sizeof s->other, "-");
    }
    if (strcmp(s->device, id) != 0) {
        return;
    }
    /* AOA START khiến phone biến mất tạm thời — không về idle ngay. */
    if (s->pending_reenum) {
        snprintf(s->phase, sizeof s->phase, "reenumerating");
        snprintf(s->reason, sizeof s->reason, "aoa-reenum");
        s->streaming = 0;
        return;
    }
    set_idle(s);
}

void usbman_expect_reenum(usbman_session_t *s)
{
    s->pending_reenum = 1;
}

void usbman_fail(usbman_session_t *s, const char *reason)
{
    snprintf(s->phase, sizeof s->phase, "failed");
    snprintf(s->reason, sizeof s->reason, "%s", reason ? reason : "failed");
    s->pending_reenum = 0;
    s->streaming = 0;
}

/* Tọa độ chuẩn hóa 0..10000 (UI map từ pixel); tăng touch_count cho debug. */
void usbman_touch(usbman_session_t *s, int x, int y, int down)
{
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > 10000) {
        x = 10000;
    }
    if (y > 10000) {
        y = 10000;
    }
    s->last_touch.x = x;
    s->last_touch.y = y;
    s->last_touch.down = down ? 1 : 0;
    if (s->touch_count < 1000000) {
        s->touch_count++;
    }
}

/* User bật/tắt stream tường minh (1/0); khác -1 (auto). */
int usbman_set_stream(usbman_session_t *s, int on)
{
    s->stream_user = on ? 1 : 0;
    refresh_stream(s);
    return s->streaming;
}

/* Serialize session → một dòng wire cho client/UI. Field escape để an toàn space. */
void usbman_state_line(const usbman_session_t *s, char *out, size_t n)
{
    char reason[USBMAN_STR_LEN * 3];
    char device[USBMAN_ID_LEN * 3];
    char parked[USBMAN_ID_LEN * 3];
    char other[USBMAN_ID_LEN * 3];
    char net[96];

    hupi_escape(s->reason, reason, sizeof reason);
    hupi_escape(s->device[0] ? s->device : "-", device, sizeof device);
    hupi_escape(s->parked[0] ? s->parked : "-", parked, sizeof parked);
    hupi_escape(s->other[0] ? s->other : "-", other, sizeof other);
    hupi_escape(s->net[0] ? s->net : "-", net, sizeof net);
    snprintf(out, n,
             "state backend=%s phase=%s streaming=%d device=%s reason=%s parked=%s net=%s "
             "touchq=%d touch=%d,%d,%d other=%s",
             s->backend, s->phase, s->streaming, device, reason, parked, net, s->touch_count,
             s->last_touch.x, s->last_touch.y, s->last_touch.down, other);
}
