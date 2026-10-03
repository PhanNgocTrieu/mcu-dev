#include "hupi_wire.h"

#include <errno.h>
#include <stdint.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef DRIVERD_PATH
#error DRIVERD_PATH
#endif
#ifndef MANAGERD_PATH
#error MANAGERD_PATH
#endif

static int g_fail;
static pid_t g_driver = -1;
static pid_t g_manager = -1;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + (uint32_t)(ts.tv_nsec / 1000000));
}

static pid_t spawn(const char *bin, const char *runtime)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        execl(bin, bin, "--runtime", runtime, (char *)NULL);
        perror(bin);
        _exit(127);
    }
    return pid;
}

static int wait_path(const char *path, int ms)
{
    uint32_t start = now_ms();
    while (now_ms() - start < (uint32_t)ms) {
        if (access(path, F_OK) == 0) {
            return 0;
        }
        usleep(20000);
    }
    return -1;
}

static int talk(hupi_peer_t *peer, const char *cmd)
{
    if (hupi_send_line(peer->fd, cmd) != 0) {
        return -1;
    }
    return 0;
}

static int line_has(const char *line, const char *a, const char *b, const char *c)
{
    if (strncmp(line, "state ", 6) != 0 || !strstr(line, a)) {
        return 0;
    }
    if (b && !strstr(line, b)) {
        return 0;
    }
    if (c && !strstr(line, c)) {
        return 0;
    }
    return 1;
}

static int until_state(hupi_peer_t *peer, const char *a, const char *b, const char *c, int ms)
{
    uint32_t start = now_ms();
    char line[1400];
    while (now_ms() - start < (uint32_t)ms) {
        struct pollfd pfd = {.fd = peer->fd, .events = POLLIN};
        int pr = poll(&pfd, 1, 50);
        int rc;
        if (pr < 0 && errno == EINTR) {
            continue;
        }
        if (pr > 0 && (pfd.revents & POLLIN)) {
            if (hupi_peer_recv(peer) != 0) {
                return -1;
            }
        }
        while ((rc = hupi_peer_pull(peer, line, sizeof line)) == 1) {
            if (line_has(line, a, b, c)) {
                return 0;
            }
        }
    }
    return -1;
}

static int read_frame(const char *runtime)
{
    char path[160];
    int fd;
    uint8_t hdr[20];
    size_t got = 0;
    uint32_t start = now_ms();
    uint32_t magic;

    hupi_sock_path(path, sizeof path, runtime, HUPI_STREAM_SOCK);
    fd = hupi_connect_unix(path);
    if (fd < 0) {
        return -1;
    }
    while (got < sizeof hdr && now_ms() - start < 2000) {
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        ssize_t n;
        if (poll(&pfd, 1, 100) <= 0) {
            continue;
        }
        n = recv(fd, hdr + got, sizeof hdr - got, 0);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        got += (size_t)n;
    }
    close(fd);
    magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    /* Lab mặc định H.264 stub; RGB shim khi HUPI_MEDIA_H264_STUB=OFF. */
    return (magic == HUPI_FRAME_MAGIC || magic == HUPI_H264_MAGIC) ? 0 : -1;
}

static void stop_daemons(void)
{
    if (g_manager > 0) {
        kill(g_manager, SIGTERM);
        waitpid(g_manager, NULL, 0);
    }
    if (g_driver > 0) {
        kill(g_driver, SIGTERM);
        waitpid(g_driver, NULL, 0);
    }
}

int main(void)
{
    char runtime[] = "/tmp/hupi-itest-XXXXXX";
    char driver_sock[160];
    char manager_sock[160];
    int fd;
    hupi_peer_t peer;

    if (!mkdtemp(runtime)) {
        perror("mkdtemp");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    g_driver = spawn(DRIVERD_PATH, runtime);
    g_manager = spawn(MANAGERD_PATH, runtime);
    hupi_sock_path(driver_sock, sizeof driver_sock, runtime, HUPI_DRIVER_SOCK);
    hupi_sock_path(manager_sock, sizeof manager_sock, runtime, HUPI_MANAGER_SOCK);
    expect(wait_path(driver_sock, 2000) == 0, "driver socket");
    expect(wait_path(manager_sock, 2000) == 0, "manager socket");
    fd = hupi_connect_unix(manager_sock);
    expect(fd >= 0, "connect manager");
    if (fd < 0) {
        stop_daemons();
        return 1;
    }
    memset(&peer, 0, sizeof peer);
    peer.fd = fd;
    expect(talk(&peer, "sub") == 0, "sub");
    expect(until_state(&peer, "phase=idle", "backend=none", NULL, 2000) == 0, "idle");
    expect(talk(&peer, "sim plug android") == 0, "plug android");
    expect(until_state(&peer, "backend=android", "phase=active", "streaming=1", 3000) == 0,
           "android streaming");
    expect(read_frame(runtime) == 0, "stream frame");
    expect(talk(&peer, "sim plug carplay") == 0, "plug carplay while busy");
    expect(until_state(&peer, "parked=sim-carplay", "backend=android", NULL, 2000) == 0, "carplay parked");
    expect(talk(&peer, "sim unplug sim-android") == 0, "unplug android");
    expect(until_state(&peer, "backend=carplay", "phase=active", "streaming=1", 3000) == 0, "carplay active");
    expect(talk(&peer, "touch 1000 2000 1") == 0, "touch");
    expect(until_state(&peer, "touch=1000,2000,1", "backend=carplay", NULL, 2000) == 0, "touch queued");
    expect(talk(&peer, "sim unplug") == 0, "unplug all");
    expect(until_state(&peer, "backend=none", "phase=idle", NULL, 2000) == 0, "idle again");
    close(fd);
    stop_daemons();
    if (g_fail) {
        fprintf(stderr, "integration failed\n");
        return 1;
    }
    printf("integration ok\n");
    return 0;
}
