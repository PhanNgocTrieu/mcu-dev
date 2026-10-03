/**
 * @file hupi_h264.h
 * @brief H.264 Annex-B stub AU + đóng gói frame wire (magic H264).
 *
 * Dùng tạm khi chưa có AASDK/MFi thật. Sau này thay payload bằng AU từ
 * aasdk::messenger / CarPlay media TCP — giữ nguyên hupi_h264_pack_frame().
 */
#ifndef HUPI_H264_H
#define HUPI_H264_H

#include "hupi_wire.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** AU stub cứng (SPS + PPS + IDR placeholder). Không đảm bảo decode đẹp — chỉ để nối pipeline. */
const uint8_t *hupi_h264_stub_au(size_t *out_len);

/**
 * Đóng gói header 20 byte + Annex-B AU vào @p out.
 * @param keyframe 1 nếu IDR/key (ghi vào field stride tạm: bit0); 0 = P-frame stub.
 * @return tổng byte đã ghi, hoặc -1 nếu buffer nhỏ / AU quá dài.
 */
int hupi_h264_pack_frame(uint8_t *out, size_t out_cap, uint32_t w, uint32_t h, int keyframe,
                         const uint8_t *au, size_t au_len);

/** Pack sẵn stub AU (keyframe=1). */
int hupi_h264_pack_stub(uint8_t *out, size_t out_cap, uint32_t w, uint32_t h);

#ifdef __cplusplus
}
#endif

#endif
