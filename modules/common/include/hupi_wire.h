#ifndef HUPI_WIRE_H
#define HUPI_WIRE_H

/**
 * @file hupi_wire.h
 * @brief Giao thức text + Unix socket dùng chung usb-driverd / usb-managerd / demo.
 *
 * Một message = một dòng kết thúc '\\n', field `key=value`.
 * Giá trị không phải token thuần → percent-encode; rỗng → "-".
 *
 * Frame video trên usb-stream.sock — header 20 byte + payload:
 *   RGB  : magic=FRM1, stride=w*3, nbytes=stride*h, payload=RGB24
 *   H.264: magic=H264, stride=0 (unused), nbytes=AU length, payload=Annex-B
 */

#include <stddef.h>
#include <stdint.h>

#define HUPI_FRAME_MAGIC 0x314D5246u /* little-endian bytes: 'F''R''M''1' */
#define HUPI_H264_MAGIC 0x34363248u  /* little-endian bytes: 'H''2''6''4' */
#define HUPI_FRAME_W 640
#define HUPI_FRAME_H 360
#define HUPI_H264_AU_MAX 65536u      /* giới hạn AU stub / một lần gửi stream */

#define HUPI_DRIVER_SOCK "usb-driver.sock"   /* dưới $HUPI_RUNTIME */
#define HUPI_MANAGER_SOCK "usb-manager.sock"
#define HUPI_STREAM_SOCK "usb-stream.sock"

/** Peer đã kết nối: buffer nhận + cờ đã subscribe sự kiện. */
typedef struct {
    int fd;
    char buf[2048];
    size_t len;
    int sub; /* 1 = đã gửi lệnh "sub", nhận broadcast */
} hupi_peer_t;

int hupi_mkdir_runtime(const char *dir);
int hupi_listen_unix(const char *path);
int hupi_connect_unix(const char *path);
int hupi_set_nonblock(int fd);
int hupi_send_line(int fd, const char *line);

/* 1 = a line was copied (newline stripped). 0 = need more bytes. -1 = dead. */
int hupi_peer_pull(hupi_peer_t *peer, char *line, size_t line_n);
/* 0 = more data or EAGAIN. -1 = peer closed. */
int hupi_peer_recv(hupi_peer_t *peer);

int hupi_kv_get(const char *line, const char *key, char *out, size_t out_n);
int hupi_kv_get_int(const char *line, const char *key, int *out);
int hupi_kv_get_uint(const char *line, const char *key, unsigned *out);

void hupi_escape(const char *in, char *out, size_t out_n);
void hupi_unescape(const char *in, char *out, size_t out_n);

int hupi_hex_encode(const uint8_t *in, size_t n, char *out, size_t out_n);
int hupi_hex_decode(const char *in, uint8_t *out, size_t cap, size_t *out_n);

void hupi_sock_path(char *out, size_t n, const char *runtime, const char *name);

#endif
