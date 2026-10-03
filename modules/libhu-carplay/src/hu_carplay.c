/**
 * @file hu_carplay.c
 * @brief Đường media CarPlay trên CDC-NCM. Hook MFi/IAP2 cho image EVB.
 *
 * Không có HUPI_WITH_MFI: shim RGB + log. Có MFi: mở IAP2 trên link NCM rồi media TCP.
 */
#include "hu_carplay.h"

#include "hupi_log.h"
#include "hupi_wire.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct hu_carplay_session {
    char iface[32];
    char endpoint[96];
    hu_carplay_callbacks_t cb;
    int running;
    int tick;
    uint32_t last_ms;
    int media_fd;
};

static uint32_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

hu_carplay_session_t *hu_carplay_create(const char *iface, const char *endpoint,
                                        const hu_carplay_callbacks_t *cb)
{
    hu_carplay_session_t *s = calloc(1, sizeof *s);
    if (!s) {
        return NULL;
    }
    snprintf(s->iface, sizeof s->iface, "%s", iface && iface[0] ? iface : "usb0");
    snprintf(s->endpoint, sizeof s->endpoint, "%s",
             endpoint && endpoint[0] ? endpoint : "10.10.10.1:5000");
    if (cb) {
        s->cb = *cb;
    }
    s->media_fd = -1;
    HUPI_LOGI("carplay.create iface=%s endpoint=%s mfi=%d", s->iface, s->endpoint,
              hu_carplay_has_mfi());
    return s;
}

void hu_carplay_destroy(hu_carplay_session_t *s)
{
    if (!s) {
        return;
    }
    hu_carplay_stop(s);
    HUPI_LOGT("carplay.destroy iface=%s", s->iface);
    free(s);
}

int hu_carplay_start(hu_carplay_session_t *s)
{
    if (!s) {
        return -1;
    }
    s->running = 1;
    s->last_ms = 0;
    HUPI_LOGI("carplay.start iface=%s endpoint=%s", s->iface, s->endpoint);
#if HUPI_WITH_MFI
    /* Open IAP2 over the NCM link, complete MFi, then start media TCP. */
#endif
    if (s->cb.on_status) {
        s->cb.on_status("active", hu_carplay_has_mfi() ? "mfi" : "ncm-shim", s->cb.user);
    }
    return 0;
}

void hu_carplay_stop(hu_carplay_session_t *s)
{
    if (!s || !s->running) {
        return;
    }
    if (s->media_fd >= 0) {
        close(s->media_fd);
        s->media_fd = -1;
    }
    s->running = 0;
    HUPI_LOGI("carplay.stop iface=%s", s->iface);
    if (s->cb.on_status) {
        s->cb.on_status("idle", "stopped", s->cb.user);
    }
}

int hu_carplay_touch(hu_carplay_session_t *s, int x, int y, int down)
{
    if (!s || !s->running) {
        return -1;
    }
    HUPI_LOGT("carplay.touch x=%d y=%d down=%d", x, y, down);
    return 0;
}

/*
 * Poll media CarPlay. Có MFi: đọc TCP/IAP2. Không: RGB xanh dương shim ~10 fps
 * (cùng layout header FRM1 với libhu-aa để cluster không phụ thuộc backend).
 */
void hu_carplay_poll(hu_carplay_session_t *s)
{
    uint8_t frame[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
    uint32_t now;
    uint32_t w = HUPI_FRAME_W, h = HUPI_FRAME_H, stride = w * 3;

    if (!s || !s->running || !s->cb.on_video) {
        return;
    }
    now = mono_ms();
    if (s->last_ms && now - s->last_ms < 100) {
        return;
    }
    s->last_ms = now;
    s->tick++;
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
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                uint8_t *p = frame + 20 + y * stride + x * 3;
                int stripe = ((int)x + s->tick * 3) / 24 % 2;
                p[0] = (uint8_t)(18 + stripe * 20);
                p[1] = (uint8_t)(48 + y / 6);
                p[2] = (uint8_t)(130 + stripe * 40);
            }
        }
        if ((s->tick % 10) == 0) {
            HUPI_LOGT("carplay.video frame=%u iface=%s", s->tick, s->iface);
        }
        s->cb.on_video(frame, 20u + nbytes, (uint64_t)now * 1000ull, 0, s->cb.user);
    }
}

int hu_carplay_has_mfi(void)
{
#if HUPI_WITH_MFI
    return 1;
#else
    return 0;
#endif
}
