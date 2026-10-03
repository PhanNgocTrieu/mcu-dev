/**
 * @file hu_carplay.h
 * @brief CarPlay media session over USB-NCM (IPv6) + IAP2/MFi boundary.
 *
 * Matches the HUPI service diagram: phone <-> CDC-NCM <-> connectivity.
 * libiap2 / libhu-mfi supply authentication when present on the EVB image.
 * Video frames leave through the same callback style as libhu-aa so
 * hu-graphicsd (or the rasp4 demo cluster) can stay backend-agnostic.
 */
#ifndef HU_CARPLAY_H
#define HU_CARPLAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hu_carplay_session hu_carplay_session_t;

typedef struct {
    void (*on_video)(const uint8_t *data, size_t len, uint64_t pts_us, int is_h264, void *user);
    void (*on_status)(const char *phase, const char *detail, void *user);
    void *user;
} hu_carplay_callbacks_t;

/**
 * @param iface Network interface bound by cdc_ncm (e.g. usb0).
 * @param endpoint Host:port for the CarPlay receiver (default 10.10.10.1:5000).
 */
hu_carplay_session_t *hu_carplay_create(const char *iface, const char *endpoint,
                                        const hu_carplay_callbacks_t *cb);
void hu_carplay_destroy(hu_carplay_session_t *s);
int hu_carplay_start(hu_carplay_session_t *s);
void hu_carplay_stop(hu_carplay_session_t *s);
int hu_carplay_touch(hu_carplay_session_t *s, int x, int y, int down);
void hu_carplay_poll(hu_carplay_session_t *s);

/** @return 1 when libiap2 + libhu-mfi were linked. */
int hu_carplay_has_mfi(void);

#ifdef __cplusplus
}
#endif

#endif
