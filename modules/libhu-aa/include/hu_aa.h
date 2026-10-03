/**
 * @file hu_aa.h
 * @brief Biên giới session media Android Auto (AASDK trên image board).
 *
 * usb-man sở hữu lifecycle thiết bị. Thư viện này sở hữu session AA:
 * nguồn video, sink input, start/stop. Có HUPI_WITH_AASDK → stack thật;
 * không thì shim vẫn emit frame để giữ đường graphics.
 */
#ifndef HU_AA_H
#define HU_AA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hu_aa_session hu_aa_session_t;

typedef struct {
    /** Called for each H.264 access unit (or RGB shim when AASDK is off). */
    void (*on_video)(const uint8_t *data, size_t len, uint64_t pts_us, int is_h264, void *user);
    void (*on_status)(const char *phase, const char *detail, void *user);
    void *user;
} hu_aa_callbacks_t;

/**
 * @brief Create a session for an already-claimed AOAP device node.
 * @param devnode Path like /dev/bus/usb/001/004, or "sim" for the lab path.
 */
hu_aa_session_t *hu_aa_create(const char *devnode, const hu_aa_callbacks_t *cb);

void hu_aa_destroy(hu_aa_session_t *s);

/** @brief Start projection channels. Returns 0 on success. */
int hu_aa_start(hu_aa_session_t *s);

/** @brief Stop projection and release endpoints. */
void hu_aa_stop(hu_aa_session_t *s);

/** @brief Inject a touch sample in 0..10000 normalized coordinates. */
int hu_aa_touch(hu_aa_session_t *s, int x, int y, int down);

/** @brief Pump I/O; call from the manager poll loop. */
void hu_aa_poll(hu_aa_session_t *s);

/** @return 1 when built against real AASDK. */
int hu_aa_has_aasdk(void);

#ifdef __cplusplus
}
#endif

#endif
