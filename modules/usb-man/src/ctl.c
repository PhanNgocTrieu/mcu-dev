/**
 * @file ctl.c
 * @brief CLI hupi-ctl — gửi một lệnh tới usb-managerd hoặc usb-driverd rồi in reply.
 *
 * Ví dụ: hupi-ctl manager status | sim plug android | stream on
 */
#include "hupi_wire.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr,
            "usage: hupi-ctl [--runtime DIR] manager|driver COMMAND...\n"
            "  hupi-ctl manager status\n"
            "  hupi-ctl manager sim plug android\n"
            "  hupi-ctl manager sim plug carplay\n"
            "  hupi-ctl manager sim unplug\n"
            "  hupi-ctl manager stream on\n");
}

int main(int argc, char **argv)
{
    const char *runtime = getenv("HUPI_RUNTIME");
    const char *which;
    const char *sockname;
    char path[160];
    char command[512];
    int fd;
    int argi = 1;
    size_t used = 0;
    hupi_peer_t peer;

    if (!runtime || !runtime[0]) {
        runtime = "/run/hupi";
    }
    if (argi < argc && strcmp(argv[argi], "--runtime") == 0 && argi + 1 < argc) {
        runtime = argv[argi + 1];
        argi += 2;
    }
    if (argc - argi < 2) {
        usage();
        return 2;
    }
    which = argv[argi++];
    if (strcmp(which, "manager") == 0) {
        sockname = HUPI_MANAGER_SOCK;
    } else if (strcmp(which, "driver") == 0) {
        sockname = HUPI_DRIVER_SOCK;
    } else {
        usage();
        return 2;
    }
    command[0] = '\0';
    for (; argi < argc; argi++) {
        size_t len = strlen(argv[argi]);
        if (used && used + 1 < sizeof command) {
            command[used++] = ' ';
        }
        if (used + len >= sizeof command) {
            fprintf(stderr, "command too long\n");
            return 2;
        }
        memcpy(command + used, argv[argi], len);
        used += len;
        command[used] = '\0';
    }
    hupi_sock_path(path, sizeof path, runtime, sockname);
    fd = hupi_connect_unix(path);
    if (fd < 0) {
        fprintf(stderr, "cannot connect to %s\n", path);
        return 1;
    }
    if (hupi_send_line(fd, command) != 0) {
        perror("send");
        close(fd);
        return 1;
    }
    memset(&peer, 0, sizeof peer);
    peer.fd = fd;
    for (;;) {
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        char line[1400];
        int rc;
        if (poll(&pfd, 1, 2000) <= 0) {
            fprintf(stderr, "timeout waiting for %s\n", path);
            close(fd);
            return 1;
        }
        if (hupi_peer_recv(&peer) != 0) {
            fprintf(stderr, "socket closed\n");
            close(fd);
            return 1;
        }
        while ((rc = hupi_peer_pull(&peer, line, sizeof line)) == 1) {
            puts(line);
            if (strncmp(line, "ok", 2) == 0 || strncmp(line, "err", 3) == 0) {
                close(fd);
                return strncmp(line, "ok", 2) == 0 ? 0 : 1;
            }
        }
        if (rc < 0) {
            close(fd);
            return 1;
        }
    }
}
