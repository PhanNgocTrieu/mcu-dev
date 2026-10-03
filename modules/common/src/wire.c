#include "hupi_wire.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

int hupi_mkdir_runtime(const char *dir)
{
    if (!dir || !dir[0]) {
        return -1;
    }
    if (mkdir(dir, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}

int hupi_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void hupi_sock_path(char *out, size_t n, const char *runtime, const char *name)
{
    snprintf(out, n, "%s/%s", runtime, name);
}

int hupi_listen_unix(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    if (strlen(path) >= sizeof addr.sun_path) {
        errno = ENAMETOOLONG;
        return -1;
    }
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    unlink(path);
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) < 0) {
        close(fd);
        unlink(path);
        return -1;
    }
    chmod(path, 0666);
    return fd;
}

int hupi_connect_unix(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    if (strlen(path) >= sizeof addr.sun_path) {
        errno = ENAMETOOLONG;
        return -1;
    }
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int hupi_send_line(int fd, const char *line)
{
    char buf[1400];
    size_t n;
    size_t off = 0;

    n = (size_t)snprintf(buf, sizeof buf, "%s\n", line);
    if (n >= sizeof buf) {
        errno = EMSGSIZE;
        return -1;
    }
    while (off < n) {
        ssize_t w = send(fd, buf + off, n - off, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

int hupi_peer_pull(hupi_peer_t *peer, char *line, size_t line_n)
{
    char *nl;
    size_t raw;
    size_t copy;

    nl = memchr(peer->buf, '\n', peer->len);
    if (!nl) {
        if (peer->len == sizeof peer->buf) {
            return -1;
        }
        return 0;
    }
    raw = (size_t)(nl - peer->buf);
    copy = raw;
    if (copy >= line_n) {
        copy = line_n - 1;
    }
    memcpy(line, peer->buf, copy);
    line[copy] = '\0';
    if (copy > 0 && line[copy - 1] == '\r') {
        line[copy - 1] = '\0';
    }
    memmove(peer->buf, nl + 1, peer->len - raw - 1);
    peer->len -= raw + 1;
    return 1;
}

int hupi_peer_recv(hupi_peer_t *peer)
{
    ssize_t n;

    if (peer->len >= sizeof peer->buf) {
        return -1;
    }
    n = recv(peer->fd, peer->buf + peer->len, sizeof peer->buf - peer->len, 0);
    if (n == 0) {
        return -1;
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        return -1;
    }
    peer->len += (size_t)n;
    return 0;
}

static const char *find_key(const char *line, const char *key)
{
    size_t klen = strlen(key);
    const char *p = line;

    while (*p) {
        if ((p == line || p[-1] == ' ') && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            return p + klen + 1;
        }
        p++;
    }
    return NULL;
}

int hupi_kv_get(const char *line, const char *key, char *out, size_t out_n)
{
    const char *val;
    const char *end;
    size_t n;

    if (!out || out_n == 0) {
        return -1;
    }
    val = find_key(line, key);
    if (!val) {
        out[0] = '\0';
        return -1;
    }
    end = strchr(val, ' ');
    n = end ? (size_t)(end - val) : strlen(val);
    if (n >= out_n) {
        n = out_n - 1;
    }
    memcpy(out, val, n);
    out[n] = '\0';
    return 0;
}

int hupi_kv_get_int(const char *line, const char *key, int *out)
{
    char tmp[64];
    char *end = NULL;
    long v;

    if (hupi_kv_get(line, key, tmp, sizeof tmp) != 0) {
        return -1;
    }
    v = strtol(tmp, &end, 0);
    if (!end || *end) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

int hupi_kv_get_uint(const char *line, const char *key, unsigned *out)
{
    char tmp[64];
    char *end = NULL;
    unsigned long v;

    if (hupi_kv_get(line, key, tmp, sizeof tmp) != 0) {
        return -1;
    }
    v = strtoul(tmp, &end, 0);
    if (!end || *end) {
        return -1;
    }
    *out = (unsigned)v;
    return 0;
}

void hupi_escape(const char *in, char *out, size_t out_n)
{
    static const char hex[] = "0123456789abcdef";
    size_t o = 0;

    if (!in) {
        in = "";
    }
    if (!in[0]) {
        if (out_n > 1) {
            out[0] = '-';
            out[1] = '\0';
        } else if (out_n) {
            out[0] = '\0';
        }
        return;
    }
    for (size_t i = 0; in[i] && o + 1 < out_n; i++) {
        unsigned char c = (unsigned char)in[i];
        int plain = isalnum(c) || c == '.' || c == '_' || c == '-' || c == ':' || c == '/';
        if (plain) {
            out[o++] = (char)c;
        } else if (o + 3 < out_n) {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xf];
        } else {
            break;
        }
    }
    out[o] = '\0';
}

static int nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void hupi_unescape(const char *in, char *out, size_t out_n)
{
    size_t o = 0;

    if (!in || (in[0] == '-' && in[1] == '\0')) {
        if (out_n) {
            out[0] = '\0';
        }
        return;
    }
    for (size_t i = 0; in[i] && o + 1 < out_n; i++) {
        if (in[i] == '%' && nibble(in[i + 1]) >= 0 && nibble(in[i + 2]) >= 0) {
            out[o++] = (char)((nibble(in[i + 1]) << 4) | nibble(in[i + 2]));
            i += 2;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

int hupi_hex_encode(const uint8_t *in, size_t n, char *out, size_t out_n)
{
    static const char hex[] = "0123456789abcdef";
    if (out_n < n * 2 + 1) {
        return -1;
    }
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 0xf];
    }
    out[n * 2] = '\0';
    return 0;
}

int hupi_hex_decode(const char *in, uint8_t *out, size_t cap, size_t *out_n)
{
    size_t len;
    size_t i;

    if (!in) {
        return -1;
    }
    len = strlen(in);
    if (len % 2 || len / 2 > cap) {
        return -1;
    }
    for (i = 0; i < len; i += 2) {
        int hi = nibble(in[i]);
        int lo = nibble(in[i + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    *out_n = len / 2;
    return 0;
}
