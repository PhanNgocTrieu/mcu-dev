/**
 * @file hu_aa.c
 * @brief Session Android Auto: AASDK khi bật cờ, không thì shim có log đầy đủ.
 *
 * AASDK thật (f1xpl/aasdk) link khi HUPI_WITH_AASDK=1. Hiện tại nhánh "real"
 * forward AU H.264 hardcode (hupi_h264_pack_stub) — thay bằng messenger sau.
 * Không AASDK và không HUPI_MEDIA_H264_STUB: RGB shim FRM1 cho demo UI.
 */
#include "hu_aa.h"

#include "hupi_h264.h"
#include "hupi_log.h"
#include "hupi_wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef HUPI_MEDIA_H264_STUB
#define HUPI_MEDIA_H264_STUB 0
#endif

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

static int use_h264_path(void)
{
#if HUPI_WITH_AASDK
    return 1;
#elif HUPI_MEDIA_H264_STUB
    return 1;
#else
    return 0;
#endif
}

/*
 * Forward một AU H.264.
 * TODO(AASDK): thay khối stub bằng:
 *   aasdk::messenger → VideoService → onIndication(payload)
 *   rồi hupi_h264_pack_frame(out, ..., keyframe, au, au_len)
 */
static void emit_h264(hu_aa_session_t *s, uint32_t now_ms)
{
    uint8_t packet[20 + 512];
    int n;
    size_t au_len = 0;
    const uint8_t *au;

#if HUPI_WITH_AASDK
    /*
     * Real path (chưa nối AASDK tree):
     * pull H.264 AUs from aasdk::messenger and forward them.
     * Tạm thời dùng AU hardcode — sửa tại đây khi có third_party/aasdk.
     */
    (void)0;
#endif
    au = hupi_h264_stub_au(&au_len);
    n = hupi_h264_pack_frame(packet, sizeof packet, HUPI_FRAME_W, HUPI_FRAME_H, 1, au, au_len);
    if (n < 0) {
        HUPI_LOGE("aa.h264 pack failed");
        return;
    }
    if ((s->tick % 10) == 0) {
        HUPI_LOGI("aa.video h264 stub frame=%u bytes=%d aasdk=%d", s->tick, n, hu_aa_has_aasdk());
    }
    s->cb.on_video(packet, (size_t)n, (uint64_t)now_ms * 1000ull, 1, s->cb.user);
}

static void emit_rgb_shim(hu_aa_session_t *s, uint32_t now_ms)
{
    uint8_t frame[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
    uint32_t w = HUPI_FRAME_W, h = HUPI_FRAME_H, stride = w * 3;
    uint32_t nbytes = stride * h;

    frame[0] = (uint8_t)HUPI_FRAME_MAGIC;
    frame[1] = (uint8_t)(HUPI_FRAME_MAGIC >> 8);
    frame[2] = (uint8_t)(HUPI_FRAME_MAGIC >> 16);
    frame[3] = (uint8_t)(HUPI_FRAME_MAGIC >> 24);
    memcpy(frame + 4, &w, 4);
    memcpy(frame + 8, &h, 4);
    memcpy(frame + 12, &stride, 4);
    memcpy(frame + 16, &nbytes, 4);
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
        HUPI_LOGT("aa.video rgb frame=%u bytes=%u", s->tick, (unsigned)(20 + nbytes));
    }
    s->cb.on_video(frame, 20u + nbytes, (uint64_t)now_ms * 1000ull, 0, s->cb.user);
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
    HUPI_LOGI("aa.create node=%s aasdk=%d h264_stub=%d", s->node, hu_aa_has_aasdk(),
              use_h264_path());
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
        const char *detail = hu_aa_has_aasdk() ? "aasdk" : (use_h264_path() ? "h264-stub" : "shim");
        s->cb.on_status("active", detail, s->cb.user);
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
    /* TODO: inject vào AASDK input channel. */
#else
    (void)x;
    (void)y;
    (void)down;
#endif
    return 0;
}

void hu_aa_poll(hu_aa_session_t *s)
{
    uint32_t now;

    if (!s || !s->running || !s->cb.on_video) {
        return;
    }
    now = mono_ms();
    if (s->last_ms && now - s->last_ms < 100) {
        return; /* ~10 fps */
    }
    s->last_ms = now;
    s->tick++;

    if (use_h264_path()) {
        emit_h264(s, now);
    } else {
        emit_rgb_shim(s, now);
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
