/**
 * @file h264_stub.c
 * @brief AU H.264 Annex-B hardcode + packer frame wire.
 *
 * Thay k_h264_stub_au / nguồn AU khi có AASDK hoặc file .h264 thật.
 * Layout header giống FRM1 nhưng magic = H264, stride dùng làm flags (bit0=key).
 */
#include "hupi_h264.h"

#include <string.h>

/*
 * Annex-B tối giản: SPS + PPS + IDR NAL (payload placeholder).
 * Mục tiêu: pipeline nhận diện is_h264 / magic H264 — chưa tối ưu decode.
 * Marker ASCII "HUPI-H264-STUB" nằm trong IDR để grep/hexdump dễ thấy.
 */
static const uint8_t k_h264_stub_au[] = {
    /* SPS (NAL type 7), Baseline-ish placeholder for 640x360 class */
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1e, 0xab, 0x40, 0xf0, 0x28,
    0x0f, 0x68, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x0c, 0x83,
    0xc5, 0x8b, 0xa8,
    /* PPS (NAL type 8) */
    0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2,
    /* IDR (NAL type 5) + marker */
    0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x80, 0x10, 0xff, 0xff, 0x00, 0x00,
    'H',  'U',  'P',  'I',  '-',  'H',  '2',  '6',  '4',  '-',  'S',  'T',
    'U',  'B',  0x00, 0x00, 0x00, 0x01, 0x09, 0x10, /* AUD end-ish padding */
};

static void put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

const uint8_t *hupi_h264_stub_au(size_t *out_len)
{
    if (out_len) {
        *out_len = sizeof k_h264_stub_au;
    }
    return k_h264_stub_au;
}

int hupi_h264_pack_frame(uint8_t *out, size_t out_cap, uint32_t w, uint32_t h, int keyframe,
                         const uint8_t *au, size_t au_len)
{
    uint32_t flags;

    if (!out || !au || au_len == 0 || au_len > HUPI_H264_AU_MAX) {
        return -1;
    }
    if (out_cap < 20u + au_len) {
        return -1;
    }
    flags = keyframe ? 1u : 0u;
    put_u32_le(out + 0, HUPI_H264_MAGIC);
    put_u32_le(out + 4, w);
    put_u32_le(out + 8, h);
    put_u32_le(out + 12, flags); /* tái dùng field stride làm flags */
    put_u32_le(out + 16, (uint32_t)au_len);
    memcpy(out + 20, au, au_len);
    return (int)(20u + au_len);
}

int hupi_h264_pack_stub(uint8_t *out, size_t out_cap, uint32_t w, uint32_t h)
{
    size_t au_len = 0;
    const uint8_t *au = hupi_h264_stub_au(&au_len);
    return hupi_h264_pack_frame(out, out_cap, w, h, 1, au, au_len);
}
