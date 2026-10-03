/**
 * @file managerd.c
 * @brief Daemon usb-managerd — chính sách session + media projection.
 *
 * Luồng tổng:
 *   usb-driverd  --(dev events / ctrl replies)-->  usb-managerd
 *        ^                                              |
 *        |         claim/ctrl (AOA)                     | state / touch / stream
 *        +----------------------------------------------+
 *                                                       |
 *                              demo apps / hupi-ctl <---+
 *                              + video trên usb-stream.sock
 *
 * Chỉ một backend projection tại một thời điểm (Android Auto hoặc CarPlay).
 * Media thật nằm ở libhu-aa / libhu-carplay; không có AASDK/MFi thì dùng shim RGB.
 */
#include "hupi_wire.h"
#include "hupi_log.h"
#include "hu_aa.h"
#include "hu_carplay.h"
#include "usbman.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PEER 8
#define QMAX 24

/* Một lệnh chờ gửi xuống usb-driverd; aoa=1 để xử lý lỗi/re-enum đặc biệt. */
typedef struct {
    char line[USBMAN_LINE_LEN];
    int aoa;
} qitem;

static volatile sig_atomic_t g_stop;
static usbman_session_t g_session;
static hupi_peer_t g_driver;           /* kết nối tới usb-driverd */
static hupi_peer_t g_peers[MAX_PEER];  /* client: panel, hupi-ctl, ... */
static int g_listen = -1;              /* usb-manager.sock */
static int g_stream_listen = -1;       /* usb-stream.sock */
static int g_stream_fd = -1;           /* một consumer video tại một thời điểm */
static char g_driver_path[128];
static char g_manager_path[128];
static char g_stream_path[128];

/* Hàng đợi lệnh tuần tự gửi driver (claim/ctrl/sim...); g_waiting = đang chờ ok/err */
static qitem g_q[QMAX];
static int g_qh;
static int g_qn;
static int g_waiting;

static uint8_t g_frame[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
static size_t g_frame_len;
static size_t g_frame_off;
static int g_frame_busy;
static uint32_t g_last_frame_ms;
static int g_tick;
static hu_aa_session_t *g_aa;
static hu_carplay_session_t *g_cp;
static uint8_t g_media_frame[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
static size_t g_media_len;
static int g_media_ready;

/* Callback từ lib media: giữ frame mới nhất, consider_frame sẽ đẩy ra stream sock. */
static void media_on_video(const uint8_t *data, size_t len, uint64_t pts_us, int is_h264, void *user)
{
    (void)pts_us;
    (void)is_h264;
    (void)user;
    if (len > sizeof g_media_frame) {
        return;
    }
    memcpy(g_media_frame, data, len);
    g_media_len = len;
    g_media_ready = 1;
}

static void media_on_status(const char *phase, const char *detail, void *user)
{
    (void)user;
    HUPI_LOGI("media.status phase=%s detail=%s", phase, detail);
}

static void media_stop(void)
{
    if (g_aa) {
        HUPI_LOGI("media.stop android");
        hu_aa_destroy(g_aa);
        g_aa = NULL;
    }
    if (g_cp) {
        HUPI_LOGI("media.stop carplay");
        hu_carplay_destroy(g_cp);
        g_cp = NULL;
    }
    g_media_ready = 0;
}

/*
 * Đồng bộ media với session:
 *  - chưa active / user tắt stream → destroy backend
 *  - android active + chưa có g_aa → create/start libhu-aa
 *  - carplay active + chưa có g_cp → create/start libhu-carplay (theo net iface)
 * media_stop() trước khi create để chắc chỉ một backend sống.
 */
static void media_sync(void)
{
    if (strcmp(g_session.phase, "active") != 0 || !g_session.streaming) {
        if (g_aa || g_cp) {
            media_stop();
        }
        return;
    }
    if (!strcmp(g_session.backend, "android") && !g_aa) {
        hu_aa_callbacks_t cb = {.on_video = media_on_video, .on_status = media_on_status};
        media_stop();
        g_aa = hu_aa_create(g_session.device, &cb);
        if (g_aa) {
            HUPI_LOGI("media.start android device=%s aasdk=%d", g_session.device, hu_aa_has_aasdk());
            hu_aa_start(g_aa);
        }
    } else if (!strcmp(g_session.backend, "carplay") && !g_cp) {
        hu_carplay_callbacks_t cb = {.on_video = media_on_video, .on_status = media_on_status};
        media_stop();
        g_cp = hu_carplay_create(g_session.net, NULL, &cb);
        if (g_cp) {
            HUPI_LOGI("media.start carplay net=%s mfi=%d", g_session.net, hu_carplay_has_mfi());
            hu_carplay_start(g_cp);
        }
    }
}


static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + (uint32_t)(ts.tv_nsec / 1000000));
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Đưa lệnh vào hàng đợi gửi usb-driverd (tuần tự, một lệnh một lúc). */
static int enqueue(const char *line, int aoa)
{
    int slot;
    if (g_qn >= QMAX) {
        return -1;
    }
    slot = (g_qh + g_qn) % QMAX;
    snprintf(g_q[slot].line, sizeof g_q[slot].line, "%s", line);
    g_q[slot].aoa = aoa;
    g_qn++;
    return 0;
}

static void pop_q(void)
{
    if (g_qn <= 0) {
        return;
    }
    g_qh = (g_qh + 1) % QMAX;
    g_qn--;
}

/*
 * Khi AOA fail: bỏ các bước AOA còn lại trong queue.
 * Nếu đang chờ reply của lệnh đầu (g_waiting), giữ item đó để finish_driver_reply
 * không lệch với reply sắp tới từ driver.
 */
static void drop_aoa_queued(void)
{
    qitem kept[QMAX];
    int n = 0;
    int waiting_head = g_waiting;

    for (int i = 0; i < g_qn; i++) {
        qitem item = g_q[(g_qh + i) % QMAX];
        if (item.aoa && !(waiting_head && i == 0)) {
            continue; /* drop AOA chưa gửi / không phải head đang chờ */
        }
        kept[n++] = item;
    }
    g_qh = 0;
    g_qn = n;
    memcpy(g_q, kept, sizeof kept);
}

/* Broadcast dòng `state ...` cho client đã sub + đồng bộ media backend. */
static void publish(void)
{
    char line[512];
    usbman_state_line(&g_session, line, sizeof line);
    HUPI_LOGI("%s", line);
    media_sync();
    for (int i = 0; i < MAX_PEER; i++) {
        if (g_peers[i].fd >= 0 && g_peers[i].sub) {
            if (hupi_send_line(g_peers[i].fd, line) != 0) {
                close(g_peers[i].fd);
                g_peers[i].fd = -1;
                g_peers[i].sub = 0;
                g_peers[i].len = 0;
            }
        }
    }
}

/* Chặn injection: chỉ cho phép tên iface [A-Za-z0-9_.] trước khi exec `ip`. */
static int iface_ok(const char *iface)
{
    if (!iface || !iface[0] || strcmp(iface, "-") == 0) {
        return 0;
    }
    for (const char *p = iface; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!isalnum(c) && c != '_' && c != '.') {
            return 0;
        }
    }
    return 1;
}

/* CarPlay cần iface NCM up trước khi media/IAP2 chạy — fork+exec `ip link set`. */
static void link_up(const char *iface)
{
    pid_t pid;
    int status = 0;

    if (!iface_ok(iface)) {
        return;
    }
    pid = fork();
    if (pid < 0) {
        return;
    }
    if (pid == 0) {
        execlp("ip", "ip", "link", "set", iface, "up", (char *)NULL);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    fprintf(stderr, "usb-managerd: ip link set %s up -> %d\n", iface, status);
}

/* Xếp chuỗi AOA (claim → GET_PROTOCOL → 6 string → START) vào queue driver. */
static void start_aoa(const char *id)
{
    char lines[9][USBMAN_LINE_LEN];
    int n = usbman_aoa_build(id, lines, 9);
    HUPI_LOGI("aoa.plan id=%s steps=%d", id, n);
    if (n < 0) {
        usbman_fail(&g_session, "aoa-build");
        publish();
        return;
    }
    for (int i = 0; i < n; i++) {
        if (enqueue(lines[i], 1) != 0) {
            usbman_fail(&g_session, "aoa-queue");
            publish();
            return;
        }
    }
}

static void apply_action(usbman_action_t act, const usbman_dev_t *dev)
{
    if (act == USBMAN_ACT_AOA) {
        start_aoa(dev->id);
    } else if (act == USBMAN_ACT_LINK_UP) {
        link_up(dev->net);
    }
}

/* Cache thiết bị còn cắm — dùng khi session về idle để promote máy projection khác. */
#define KNOWN_MAX 8
static usbman_dev_t g_known[KNOWN_MAX];
static int g_nknown;

static void remember_dev(const usbman_dev_t *dev)
{
    for (int i = 0; i < g_nknown; i++) {
        if (strcmp(g_known[i].id, dev->id) == 0) {
            g_known[i] = *dev;
            return;
        }
    }
    if (g_nknown < KNOWN_MAX) {
        g_known[g_nknown++] = *dev;
    }
}

static void forget_dev(const char *id)
{
    for (int i = 0; i < g_nknown; i++) {
        if (strcmp(g_known[i].id, id) == 0) {
            g_known[i] = g_known[g_nknown - 1];
            g_nknown--;
            return;
        }
    }
}

static int kind_is_projection(usbman_kind_t kind)
{
    return kind == USBMAN_KIND_ANDROID || kind == USBMAN_KIND_ANDROID_AOAP ||
           kind == USBMAN_KIND_CARPLAY || kind == USBMAN_KIND_APPLE_WAIT ||
           kind == USBMAN_KIND_APPLE_IPHETH;
}

/* Sau khi thiết bị projection rút: nếu còn máy khác, nhận session lại. */
static void promote_idle(void)
{
    if (strcmp(g_session.phase, "idle") != 0) {
        return;
    }
    for (int i = 0; i < g_nknown; i++) {
        if (!kind_is_projection(usbman_classify(&g_known[i]))) {
            continue;
        }
        usbman_action_t act = usbman_on_device(&g_session, &g_known[i]);
        publish();
        apply_action(act, &g_known[i]);
        return;
    }
}

/* Xử lý sự kiện plug/unplug từ usb-driverd → cập nhật session + hành động. */
static void on_driver_dev(const char *line)
{
    char id[USBMAN_ID_LEN];
    usbman_dev_t dev;
    usbman_action_t act;

    if (strncmp(line, "dev gone ", 9) == 0) {
        if (hupi_kv_get(line, "id", id, sizeof id) == 0) {
            usbman_on_gone(&g_session, id); /* có thể → reenumerating hoặc idle */
            forget_dev(id);
            publish();
            promote_idle(); /* nếu idle, nhận máy projection còn lại */
        }
        return;
    }
    if (usbman_parse_dev(line, &dev) != 0) {
        return;
    }
    remember_dev(&dev);
    act = usbman_on_device(&g_session, &dev); /* policy thuần — không I/O */
    publish();                                /* broadcast state + media_sync */
    apply_action(act, &dev);                  /* AOA queue hoặc ip link up */
}

/* Parse reply GET_PROTOCOL (AOA req 51): 2 byte LE = số protocol hỗ trợ. */
static int protocol_of(const char *line)
{
    const char *hex = strstr(line, "ok ctrl ");
    uint8_t data[8];
    size_t n = 0;
    if (!hex) {
        return -1;
    }
    hex += 8;
    if (hupi_hex_decode(hex, data, sizeof data, &n) != 0 || n < 2) {
        return -1;
    }
    return data[0] | (data[1] << 8);
}

/*
 * Nhận ok/err khớp lệnh đầu queue.
 * AOA err → fail session + drop các bước AOA còn lại.
 * GET_PROTOCOL (req 51) ok nhưng protocol < 1 → phone không hỗ trợ AOA.
 */
static void finish_driver_reply(const char *line)
{
    qitem head;
    int ok;
    int err;

    if (!g_waiting || g_qn == 0) {
        return; /* không có lệnh đang chờ → bỏ qua (có thể là noise) */
    }
    ok = strncmp(line, "ok", 2) == 0 && (line[2] == '\0' || line[2] == ' ');
    err = strncmp(line, "err", 3) == 0 && (line[3] == '\0' || line[3] == ' ');
    if (!ok && !err) {
        return; /* chưa phải reply cuối (vd. dòng trung gian) */
    }
    head = g_q[g_qh];
    g_waiting = 0;
    pop_q();
    if (head.aoa && err) {
        usbman_fail(&g_session, "aoa-ctrl");
        drop_aoa_queued();
        publish();
        return;
    }
    if (head.aoa && ok && strstr(head.line, " 51 ") != NULL) {
        int protocol = protocol_of(line);
        if (protocol < 1) {
            usbman_fail(&g_session, "aoa-protocol");
            drop_aoa_queued();
            publish();
        }
    }
}

/*
 * Gửi đúng một lệnh đầu queue xuống driver, rồi chờ ok/err.
 * Trước AOA START (req 53): set pending_reenum — phone sẽ biến mất tạm thời.
 */
static void pump(void)
{
    if (g_waiting || g_qn == 0 || g_driver.fd < 0) {
        return;
    }
    if (g_q[g_qh].aoa && strstr(g_q[g_qh].line, " 53 ") != NULL) {
        usbman_expect_reenum(&g_session);
    }
    if (hupi_send_line(g_driver.fd, g_q[g_qh].line) != 0) {
        return;
    }
    g_waiting = 1;
}

static void close_driver(void)
{
    if (g_driver.fd >= 0) {
        close(g_driver.fd);
    }
    g_driver.fd = -1;
    g_driver.len = 0;
    g_driver.sub = 0;
    /* Queue vô nghĩa khi mất driver — xóa hết để tránh gửi nhầm khi reconnect. */
    g_waiting = 0;
    g_qh = 0;
    g_qn = 0;
}

/* Kết nối lazy tới usb-driverd; ngay sau đó enqueue "sub" để nhận snapshot + events. */
static void try_connect_driver(void)
{
    int fd;
    if (g_driver.fd >= 0) {
        return;
    }
    fd = hupi_connect_unix(g_driver_path);
    if (fd < 0) {
        return; /* driver chưa lên — thử lại vòng poll sau */
    }
    hupi_set_nonblock(fd);
    memset(&g_driver, 0, sizeof g_driver);
    g_driver.fd = fd;
    enqueue("sub", 0);
    HUPI_LOGI("connected to usb-driverd");
}

/* Frame demo RGB (header 20 byte + pixel) khi media shim chưa kịp hoặc không có. */
static void fill_frame(int carplay)
{
    uint32_t w = HUPI_FRAME_W;
    uint32_t h = HUPI_FRAME_H;
    uint32_t stride = w * 3;
    uint8_t *pix;
    int bar;

    g_frame_len = 20u + stride * h;
    put_u32(g_frame + 0, HUPI_FRAME_MAGIC);
    put_u32(g_frame + 4, w);
    put_u32(g_frame + 8, h);
    put_u32(g_frame + 12, stride);
    put_u32(g_frame + 16, stride * h);
    pix = g_frame + 20;
    bar = (g_tick * 6) % (int)w;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = pix + (y * stride) + x * 3;
            int stripe = ((int)x + g_tick * 3) / 24 % 2;
            if (carplay) {
                p[0] = (uint8_t)(18 + stripe * 20);
                p[1] = (uint8_t)(48 + y / 6);
                p[2] = (uint8_t)(130 + stripe * 40);
            } else {
                p[0] = (uint8_t)(16 + y / 8);
                p[1] = (uint8_t)(110 + stripe * 40);
                p[2] = (uint8_t)(48);
            }
            if ((int)x >= bar && (int)x < bar + 28) {
                p[0] = 240;
                p[1] = 240;
                p[2] = 240;
            }
        }
    }
}

/*
 * Chuẩn bị frame để gửi:
 *  1) poll media backend (có thể set g_media_ready)
 *  2) cần có consumer stream + không đang gửi dở + session streaming
 *  3) ưu tiên frame media; không thì RGB shim ~10 fps (100 ms)
 */
static void consider_frame(void)
{
    uint32_t t;
    int carplay;

    if (g_aa) {
        hu_aa_poll(g_aa);
    }
    if (g_cp) {
        hu_carplay_poll(g_cp);
    }
    if (g_stream_fd < 0 || g_frame_busy || !g_session.streaming) {
        return;
    }
    if (g_media_ready) {
        memcpy(g_frame, g_media_frame, g_media_len);
        g_frame_len = g_media_len;
        g_media_ready = 0;
        g_frame_off = 0;
        g_frame_busy = 1; /* đánh dấu có dữ liệu chờ POLLOUT */
        g_last_frame_ms = now_ms();
        return;
    }
    t = now_ms();
    if (g_last_frame_ms && t - g_last_frame_ms < 100) {
        return; /* rate-limit shim */
    }
    g_last_frame_ms = t;
    g_tick++;
    carplay = strcmp(g_session.backend, "carplay") == 0;
    fill_frame(carplay);
    g_frame_off = 0;
    g_frame_busy = 1;
}

/* Gửi dần g_frame (non-blocking); xong một frame thì g_frame_busy=0. */
static void drain_stream_write(void)
{
    ssize_t n;
    if (!g_frame_busy || g_stream_fd < 0) {
        return;
    }
    n = send(g_stream_fd, g_frame + g_frame_off, g_frame_len - g_frame_off, MSG_NOSIGNAL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return; /* thử lại khi socket writable */
        }
        close(g_stream_fd);
        g_stream_fd = -1;
        g_frame_busy = 0;
        return;
    }
    g_frame_off += (size_t)n;
    if (g_frame_off >= g_frame_len) {
        g_frame_busy = 0; /* sẵn sàng frame kế */
    }
}

static void close_peer(int index)
{
    if (g_peers[index].fd >= 0) {
        close(g_peers[index].fd);
    }
    g_peers[index].fd = -1;
    g_peers[index].len = 0;
    g_peers[index].sub = 0;
}

/*
 * Lệnh trên usb-manager.sock:
 *   sub | status | stream on/off | touch x y down | sim ... (proxy sang driver)
 */
static void handle_client(int index, const char *line)
{
    int fd = g_peers[index].fd;
    char state[512];

    if (strcmp(line, "sub") == 0) {
        g_peers[index].sub = 1;
        usbman_state_line(&g_session, state, sizeof state);
        hupi_send_line(fd, state);
        hupi_send_line(fd, "ok");
        return;
    }
    if (strcmp(line, "status") == 0) {
        usbman_state_line(&g_session, state, sizeof state);
        hupi_send_line(fd, state);
        hupi_send_line(fd, "ok");
        return;
    }
    if (strncmp(line, "sim ", 4) == 0) {
        /* Proxy nguyên dòng sang driver (sim plug/unplug...) qua queue. */
        if (g_driver.fd < 0 || enqueue(line, 0) != 0) {
            hupi_send_line(fd, "err driver");
            return;
        }
        hupi_send_line(fd, "ok");
        return;
    }
    if (strcmp(line, "stream on") == 0 || strcmp(line, "stream off") == 0) {
        /* line[7..]="on"/"off" sau "stream " */
        usbman_set_stream(&g_session, line[7] == 'o' && line[8] == 'n');
        publish();
        hupi_send_line(fd, "ok");
        return;
    }
    if (strncmp(line, "touch ", 6) == 0) {
        int x = 0, y = 0, down = 0;
        if (sscanf(line + 6, "%d %d %d", &x, &y, &down) != 3) {
            hupi_send_line(fd, "err touch");
            return;
        }
        /* Lưu vào session (UI) + forward vào media backend nếu đang chạy. */
        usbman_touch(&g_session, x, y, down);
        if (g_aa) {
            hu_aa_touch(g_aa, x, y, down);
        }
        if (g_cp) {
            hu_carplay_touch(g_cp, x, y, down);
        }
        HUPI_LOGT("touch x=%d y=%d down=%d", x, y, down);
        publish();
        hupi_send_line(fd, "ok");
        return;
    }
    hupi_send_line(fd, "err unknown");
}

static void drain_client(int index)
{
    char line[512];
    if (hupi_peer_recv(&g_peers[index]) != 0) {
        close_peer(index);
        return;
    }
    for (;;) {
        int rc = hupi_peer_pull(&g_peers[index], line, sizeof line);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            close_peer(index);
            break;
        }
        handle_client(index, line);
    }
}

static void drain_driver(void)
{
    char line[1400];
    if (hupi_peer_recv(&g_driver) != 0) {
        HUPI_LOGW("usb-driverd closed");
        close_driver();
        return;
    }
    for (;;) {
        int rc = hupi_peer_pull(&g_driver, line, sizeof line);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            close_driver();
            break;
        }
        /* Hai loại message từ driver: sự kiện thiết bị vs reply lệnh. */
        if (strncmp(line, "dev ", 4) == 0) {
            on_driver_dev(line);
        } else {
            finish_driver_reply(line);
        }
    }
}

static void accept_client(void)
{
    int fd = accept(g_listen, NULL, NULL);
    if (fd < 0) {
        return;
    }
    hupi_set_nonblock(fd);
    for (int i = 0; i < MAX_PEER; i++) {
        if (g_peers[i].fd < 0) {
            memset(&g_peers[i], 0, sizeof g_peers[i]);
            g_peers[i].fd = fd;
            return;
        }
    }
    close(fd);
}

/* Chỉ một consumer video — client mới thay thế client cũ. */
static void accept_stream(void)
{
    int fd = accept(g_stream_listen, NULL, NULL);
    if (fd < 0) {
        return;
    }
    hupi_set_nonblock(fd);
    if (g_stream_fd >= 0) {
        close(g_stream_fd);
    }
    g_stream_fd = fd;
    g_frame_busy = 0; /* bắt đầu frame mới cho consumer mới */
}

static const char *runtime_arg(int argc, char **argv)
{
    const char *runtime = getenv("HUPI_RUNTIME");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return runtime && runtime[0] ? runtime : "/run/hupi";
}

int main(int argc, char **argv)
{
    const char *runtime = runtime_arg(argc, argv);
    hupi_log_level_t level = HUPI_LOG_TRACE;
    struct sigaction sa;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--log-level") && i + 1 < argc) {
            hupi_log_level_from_str(argv[i + 1], &level);
        }
    }
    hupi_log_open("usb-man", level);

    usbman_session_init(&g_session);
    g_driver.fd = -1;
    for (int i = 0; i < MAX_PEER; i++) {
        g_peers[i].fd = -1;
    }
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (hupi_mkdir_runtime(runtime) != 0) {
        fprintf(stderr, "usb-managerd: cannot create %s\n", runtime);
        return 1;
    }
    hupi_sock_path(g_driver_path, sizeof g_driver_path, runtime, HUPI_DRIVER_SOCK);
    hupi_sock_path(g_manager_path, sizeof g_manager_path, runtime, HUPI_MANAGER_SOCK);
    hupi_sock_path(g_stream_path, sizeof g_stream_path, runtime, HUPI_STREAM_SOCK);
    g_listen = hupi_listen_unix(g_manager_path);
    g_stream_listen = hupi_listen_unix(g_stream_path);
    if (g_listen < 0 || g_stream_listen < 0) {
        fprintf(stderr, "usb-managerd: listen failed under %s\n", runtime);
        return 1;
    }
    hupi_set_nonblock(g_listen);
    hupi_set_nonblock(g_stream_listen);
    HUPI_LOGI("listen manager=%s stream=%s", g_manager_path, g_stream_path);

    /* map: -1 manager listen, -2 stream listen, -3 driver, -4 stream client, >=0 peer */
    while (!g_stop) {
        struct pollfd pfds[4 + MAX_PEER];
        int np = 0;
        int map[4 + MAX_PEER];

        /* Trước poll: đảm bảo có driver, đẩy lệnh queue, chuẩn bị frame. */
        try_connect_driver();
        pump();
        consider_frame();

        pfds[np].fd = g_listen;
        pfds[np].events = POLLIN;
        map[np] = -1;
        np++;
        pfds[np].fd = g_stream_listen;
        pfds[np].events = POLLIN;
        map[np] = -2;
        np++;
        if (g_driver.fd >= 0) {
            pfds[np].fd = g_driver.fd;
            pfds[np].events = POLLIN;
            map[np] = -3;
            np++;
        }
        if (g_stream_fd >= 0) {
            /* POLLOUT chỉ khi đang có frame dở — tránh wake liên tục. */
            pfds[np].fd = g_stream_fd;
            pfds[np].events = POLLIN | (g_frame_busy ? POLLOUT : 0);
            map[np] = -4;
            np++;
        }
        for (int i = 0; i < MAX_PEER; i++) {
            if (g_peers[i].fd >= 0) {
                pfds[np].fd = g_peers[i].fd;
                pfds[np].events = POLLIN;
                map[np] = i;
                np++;
            }
        }
        if (poll(pfds, (nfds_t)np, 50) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        for (int i = 0; i < np; i++) {
            if (pfds[i].revents == 0) {
                continue;
            }
            if (map[i] == -1 && (pfds[i].revents & POLLIN)) {
                accept_client();
            } else if (map[i] == -2 && (pfds[i].revents & POLLIN)) {
                accept_stream();
            } else if (map[i] == -3) {
                drain_driver();
            } else if (map[i] == -4) {
                if (pfds[i].revents & (POLLERR | POLLHUP)) {
                    close(g_stream_fd);
                    g_stream_fd = -1;
                    g_frame_busy = 0;
                } else {
                    if (pfds[i].revents & POLLIN) {
                        char discard[64];
                        /* Stream là one-way; đọc để phát hiện peer đóng (EOF). */
                        if (recv(g_stream_fd, discard, sizeof discard, 0) == 0) {
                            close(g_stream_fd);
                            g_stream_fd = -1;
                            g_frame_busy = 0;
                        }
                    }
                    if (pfds[i].revents & POLLOUT) {
                        drain_stream_write();
                    }
                }
            } else if (map[i] >= 0) {
                drain_client(map[i]);
            }
        }
        pump(); /* reply vừa về có thể mở slot gửi lệnh kế */
    }
    unlink(g_manager_path);
    unlink(g_stream_path);
    return 0;
}
