/**
 * @file aoa.c
 * @brief Xây chuỗi lệnh AOA gửi qua usb-driverd (Android Open Accessory).
 *
 * Thứ tự chuẩn Google AOA:
 *   1. claim thiết bị
 *   2. GET_PROTOCOL  (bm=0xC0=192, bRequest=51) — IN, 2 byte
 *   3. SEND_STRING × 6 (bm=0x40=64, bRequest=52, wIndex = index chuỗi)
 *   4. START         (bm=0x40=64, bRequest=53) — phone re-enum thành AOAP
 *
 * Sáu chuỗi: manufacturer, model, description, version, URI, serial.
 */
#include "usbman.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>

static const char *k_strings[6] = {
    "HUPI",                 /* 0 manufacturer */
    "Raspberry Pi 4",       /* 1 model */
    "Android Auto",         /* 2 description */
    "1.0",                  /* 3 version */
    "https://hupi.local/aa",/* 4 URI */
    "HUPI-RPI4",            /* 5 serial */
};

int usbman_aoa_build(const char *id, char lines[][USBMAN_LINE_LEN], int cap)
{
    int n = 0;
    char hex[160];

    if (cap < 9 || !id) {
        return -1;
    }
    snprintf(lines[n++], USBMAN_LINE_LEN, "claim %s", id);
    /* vendor-specific IN: GET_PROTOCOL */
    snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 192 51 0 0 2", id);
    for (int i = 0; i < 6; i++) {
        size_t len = strlen(k_strings[i]);
        if (hupi_hex_encode((const uint8_t *)k_strings[i], len, hex, sizeof hex) != 0) {
            return -1;
        }
        /* vendor-specific OUT: SEND_STRING, wIndex = i */
        snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 64 52 0 %d %zu %s", id, i, len, hex);
    }
    /* vendor-specific OUT: START accessory mode */
    snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 64 53 0 0 0", id);
    return n;
}
