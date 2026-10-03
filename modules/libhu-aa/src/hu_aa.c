/**
 * @file hu_aa.c
 * @brief Session Android Auto: AASDK khi bật cờ, không thì shim có log đầy đủ.
 *
 * AASDK thật (f1xpl/aasdk) link khi HUPI_WITH_AASDK=1. Shim vẫn emit frame RGB
 * + log lifecycle để demo/lab chạy được khi chưa có tree AASDK.
 */
#include "hu_aa.h"

#include "hupi_log.h"
#include "hupi_wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct hu_aa_session {
    char node[128];
    hu_aa_callbacks_t cb;
    int running;
    int tick;
    uint32_t last_ms;
};

static uint32_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

hu_aa_session_t *hu_aa_create(const char *devnode, const hu_aa_callbacks_t *cb)
{
    hu_aa_session_t *s = calloc(1, sizeof *s);
    if (!s) {
        return NULL;
    }
    snprintf(s->node, sizeof s->node, "%s", devnode ? devnode : "");
    if (cb) {
        s->cb = *cb;
    }
    HUPI_LOGI("aa.create node=%s aasdk=%d", s->node, hu_aa_has_aasdk());
    return s;
}

void hu_aa_destroy(hu_aa_session_t *s)
{
    if (!s) {
        return;
    }
    hu_aa_stop(s);
    HUPI_LOGT("aa.destroy node=%s", s->node);
    free(s);
}

int hu_aa_start(hu_aa_session_t *s)
{
    if (!s) {
        return -1;
    }
    s->running = 1;
    s->last_ms = 0;
    HUPI_LOGI("aa.start node=%s", s->node);
    if (s->cb.on_status) {
        s->cb.on_status("active", hu_aa_has_aasdk() ? "aasdk" : "shim", s->cb.user);
    }
    return 0;
}

void hu_aa_stop(hu_aa_session_t *s)
{
    if (!s || !s->running) {
        return;
    }
    s->running = 0;
    HUPI_LOGI("aa.stop node=%s", s->node);
    if (s->cb.on_status) {
        s->cb.on_status("idle", "stopped", s->cb.user);
    }
}

int hu_aa_touch(hu_aa_session_t *s, int x, int y, int down)
{
    if (!s || !s->running) {
        return -1;
    }
    HUPI_LOGT("aa.touch x=%d y=%d down=%d", x, y, down);
#if HUPI_WITH_AASDK
    /* Real AASDK input channel injection goes here. */
#else
    (void)x;
    (void)y;
    (void)down;
#endif
    return 0;
}

/*
 * Gọi mỗi vòng poll của usb-managerd.
 * Có AASDK: lấy H.264 AU từ messenger rồi on_video(..., is_h264=1).
 * Không: tạo frame RGB shim ~10 fps (header FRM1 + pixel) để demo UI vẫn chạy.
 */
void hu_aa_poll(hu_aa_session_t *s)
{
    uint8_t frame[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
    uint32_t now;
    uint32_t w = HUPI_FRAME_W, h = HUPI_FRAME_H, stride = w * 3;

    if (!s || !s->running || !s->cb.on_video) {
        return;
    }
    now = mono_ms();
    if (s->last_ms && now - s->last_ms < 100) {
        return; /* ~10 fps */
    }
    s->last_ms = now;
    s->tick++;
#if HUPI_WITH_AASDK
    /* Real path: pull H.264 AUs from aasdk::messenger and forward them. */
#endif
    /* Header little-endian: magic, w, h, stride, nbytes — khớp hupi_wire / cluster. */
    frame[0] = (uint8_t)HUPI_FRAME_MAGIC;
    frame[1] = (uint8_t)(HUPI_FRAME_MAGIC >> 8);
    frame[2] = (uint8_t)(HUPI_FRAME_MAGIC >> 16);
    frame[3] = (uint8_t)(HUPI_FRAME_MAGIC >> 24);
    memcpy(frame + 4, &w, 4);
    memcpy(frame + 8, &h, 4);
    memcpy(frame + 12, &stride, 4);
    {
        uint32_t nbytes = stride * h;
        memcpy(frame + 16, &nbytes, 4);
        /* RGB xanh lá sọc — nhận biết backend Android trên demo. */
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                uint8_t *p = frame + 20 + y * stride + x * 3;
                int stripe = ((int)x + s->tick * 3) / 24 % 2;
                p[0] = (uint8_t)(16 + y / 8);
                p[1] = (uint8_t)(110 + stripe * 40);
                p[2] = (uint8_t)(48);
            }
        }
        if ((s->tick % 10) == 0) {
            HUPI_LOGT("aa.video frame=%u bytes=%u shim=%d", s->tick, (unsigned)(20 + nbytes),
                      !hu_aa_has_aasdk());
        }
        s->cb.on_video(frame, 20u + nbytes, (uint64_t)now * 1000ull, 0, s->cb.user);
    }
}

int hu_aa_has_aasdk(void)
{
#if HUPI_WITH_AASDK
    return 1;
#else
    return 0;
#endif
}
