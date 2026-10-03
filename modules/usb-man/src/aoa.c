#include "usbman.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>

static const char *k_strings[6] = {
    "HUPI",
    "Raspberry Pi 4",
    "Android Auto",
    "1.0",
    "https://hupi.local/aa",
    "HUPI-RPI4",
};

int usbman_aoa_build(const char *id, char lines[][USBMAN_LINE_LEN], int cap)
{
    int n = 0;
    char hex[160];

    if (cap < 9 || !id) {
        return -1;
    }
    snprintf(lines[n++], USBMAN_LINE_LEN, "claim %s", id);
    snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 192 51 0 0 2", id);
    for (int i = 0; i < 6; i++) {
        size_t len = strlen(k_strings[i]);
        if (hupi_hex_encode((const uint8_t *)k_strings[i], len, hex, sizeof hex) != 0) {
            return -1;
        }
        snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 64 52 0 %d %zu %s", id, i, len, hex);
    }
    snprintf(lines[n++], USBMAN_LINE_LEN, "ctrl %s 64 53 0 0 0", id);
    return n;
}
