#ifndef HUPI_WIRE_H
#define HUPI_WIRE_H

/*
 * Text protocol shared by usb-driverd, usb-managerd, and the demo apps.
 * One message is one line ending in '\n'. Fields are key=value, values
 * percent-encoded when they are not a plain token.
 */

#include <stddef.h>
#include <stdint.h>

#define HUPI_FRAME_MAGIC 0x314D5246u /* bytes F R M 1 */
#define HUPI_FRAME_W 640
#define HUPI_FRAME_H 360

#define HUPI_DRIVER_SOCK "usb-driver.sock"
#define HUPI_MANAGER_SOCK "usb-manager.sock"
#define HUPI_STREAM_SOCK "usb-stream.sock"

typedef struct {
    int fd;
    char buf[2048];
    size_t len;
    int sub;
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
