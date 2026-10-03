/**
 * @file driverd.c
 * @brief Daemon usb-driverd — lớp user-space phía USB host.
 *
 * Vai trò:
 *  - Quét sysfs + lắng nghe netlink uevent để biết thiết bị cắm/rút.
 *  - Publish sự kiện `dev ...` / `dev gone ...` cho client đã `sub`.
 *  - Cho phép một peer `claim` thiết bị rồi gửi control transfer qua usbfs
 *    (AOA GET_PROTOCOL / SEND_STRING / START) — usb-man không tự mở /dev/bus/usb.
 *  - Hỗ trợ thiết bị giả (sim) để lab không cần điện thoại thật.
 *
 * Socket mặc định: $HUPI_RUNTIME/usb-driver.sock (hoặc /run/hupi/...).
 */
#include "hupi_wire.h"
#include "hupi_log.h"
#include "usbdrv.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_DEV 48
#define MAX_PEER 8

/* Snapshot nội bộ của một USB device (thật từ sysfs hoặc sim). */
typedef struct {
    char id[80];                 /* "busnum-devnum", vd. "1-5" hoặc "sim-android" */
    uint16_t vid;
    uint16_t pid;
    char serial[USBDRV_STR_LEN];
    char mfg[USBDRV_STR_LEN];
    char prod[USBDRV_STR_LEN];
    char ifaces[USBDRV_LIST_LEN];  /* danh sách class/subclass/proto, cách nhau bởi ',' */
    char drivers[USBDRV_LIST_LEN];
    char net[USBDRV_NAME_LEN];     /* iface mạng (usb0...) nếu có CDC-NCM/ECM */
    char node[USBDRV_PATH_LEN];    /* /dev/bus/usb/BBB/DDD */
    int ncm, ecm, rndis, ipheth, storage, hid, adb, accessory, sim, apple;
    int claim_fd;                  /* fd peer đang giữ exclusive claim; -1 = trống */
    int live;
} ud_dev;

static volatile sig_atomic_t g_stop;
static ud_dev g_real[MAX_DEV]; /* thiết bị thật từ kernel */
static int g_nreal;
static ud_dev g_sim[8];        /* thiết bị giả cho demo / test */
static int g_nsim;
static hupi_peer_t g_peers[MAX_PEER];
static int g_listen = -1;
static int g_uevent = -1;
static char g_sock[128];

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void copy_field(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src ? src : "");
}

/* Serialize một device thành một dòng wire protocol (key=value). */
static void format_dev(const ud_dev *d, char *out, size_t n)
{
    char serial[USBDRV_STR_LEN * 3];
    char mfg[USBDRV_STR_LEN * 3];
    char prod[USBDRV_STR_LEN * 3];
    char ifaces[USBDRV_LIST_LEN * 3];
    char drivers[USBDRV_LIST_LEN * 3];
    char net[USBDRV_NAME_LEN * 3];
    char node[USBDRV_PATH_LEN * 3];

    hupi_escape(d->serial, serial, sizeof serial);
    hupi_escape(d->mfg, mfg, sizeof mfg);
    hupi_escape(d->prod, prod, sizeof prod);
    hupi_escape(d->ifaces, ifaces, sizeof ifaces);
    hupi_escape(d->drivers, drivers, sizeof drivers);
    hupi_escape(d->net, net, sizeof net);
    hupi_escape(d->node, node, sizeof node);
    snprintf(out, n,
             "dev id=%s vid=0x%04x pid=0x%04x serial=%s mfg=%s prod=%s ifaces=%s drivers=%s "
             "net=%s node=%s ncm=%d ecm=%d rndis=%d ipheth=%d storage=%d hid=%d adb=%d "
             "accessory=%d sim=%d apple=%d",
             d->id, d->vid, d->pid, serial, mfg, prod, ifaces, drivers, net, node, d->ncm, d->ecm,
             d->rndis, d->ipheth, d->storage, d->hid, d->adb, d->accessory, d->sim, d->apple);
}

/* Gửi sự kiện tới mọi peer đã subscribe (`sub`). */
static void broadcast(const char *line)
{
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

static void emit_dev(const ud_dev *d)
{
    char line[1400];
    format_dev(d, line, sizeof line);
    HUPI_LOGI("event plug id=%s vid=0x%04x pid=0x%04x sim=%d ncm=%d apple=%d",
              d->id, d->vid, d->pid, d->sim, d->ncm, d->apple);
    HUPI_LOGT("%s", line);
    broadcast(line);
}

static void emit_gone(const char *id)
{
    char line[160];
    snprintf(line, sizeof line, "dev gone id=%s", id);
    HUPI_LOGI("event unplug id=%s", id);
    broadcast(line);
}

/* Map snapshot sysfs → ud_dev nội bộ. id ổn định theo busnum-devnum (không dùng sys name). */
static void from_sysfs(const usbdrv_device_t *src, ud_dev *dst)
{
    memset(dst, 0, sizeof *dst);
    snprintf(dst->id, sizeof dst->id, "%d-%d", src->busnum, src->devnum);
    dst->vid = src->vendor_id;
    dst->pid = src->product_id;
    copy_field(dst->serial, sizeof dst->serial, src->serial);
    copy_field(dst->mfg, sizeof dst->mfg, src->manufacturer);
    copy_field(dst->prod, sizeof dst->prod, src->product);
    copy_field(dst->ifaces, sizeof dst->ifaces, src->interfaces);
    copy_field(dst->drivers, sizeof dst->drivers, src->drivers);
    copy_field(dst->net, sizeof dst->net, src->net_iface);
    copy_field(dst->node, sizeof dst->node, src->devnode);
    dst->ncm = src->class_ncm;
    dst->ecm = src->class_ecm;
    dst->rndis = src->class_rndis;
    dst->ipheth = src->class_ipheth;
    dst->storage = src->class_storage;
    dst->hid = src->class_hid;
    dst->adb = src->class_adb;
    dst->accessory = src->class_accessory;
    dst->apple = src->vendor_id == 0x05ac; /* Apple Inc. */
    dst->claim_fd = -1;
    dst->live = 1;
}

/* So sánh field ảnh hưởng classify/AOA; trùng thì không broadcast lại (tránh spam). */
static int same_payload(const ud_dev *a, const ud_dev *b)
{
    return a->vid == b->vid && a->pid == b->pid && a->ncm == b->ncm && a->ecm == b->ecm &&
           a->rndis == b->rndis && a->ipheth == b->ipheth && a->storage == b->storage &&
           a->hid == b->hid && a->adb == b->adb && a->accessory == b->accessory &&
           strcmp(a->net, b->net) == 0 && strcmp(a->ifaces, b->ifaces) == 0 &&
           strcmp(a->node, b->node) == 0 && strcmp(a->serial, b->serial) == 0;
}

static ud_dev *find_id(const char *id)
{
    for (int i = 0; i < g_nreal; i++) {
        if (strcmp(g_real[i].id, id) == 0) {
            return &g_real[i];
        }
    }
    for (int i = 0; i < g_nsim; i++) {
        if (strcmp(g_sim[i].id, id) == 0) {
            return &g_sim[i];
        }
    }
    return NULL;
}

/*
 * Đồng bộ lại danh sách thiết bị thật từ sysfs:
 *  - thiết bị biến mất → emit `dev gone`
 *  - mới xuất hiện hoặc đổi payload (iface/net/pid...) → emit `dev ...`
 * Giữ nguyên claim_fd nếu cùng id còn sống (AOA re-enum sẽ đổi pid/iface).
 */
static void refresh_real(void)
{
    usbdrv_device_t list[MAX_DEV];
    ud_dev next[MAX_DEV];
    size_t count = 0;
    int nnext = 0;

    if (usbdrv_enum_root("/sys/bus/usb/devices", list, MAX_DEV, &count) != 0) {
        return;
    }

    /* Bước 1: dựng danh sách mới từ sysfs, bỏ id rỗng / 0-0. */
    for (size_t i = 0; i < count && nnext < MAX_DEV; i++) {
        from_sysfs(&list[i], &next[nnext]);
        if (next[nnext].id[0] == '\0' || strcmp(next[nnext].id, "0-0") == 0) {
            continue;
        }
        nnext++;
    }

    /* Bước 2: id cũ không còn trong next → unplug. */
    for (int i = 0; i < g_nreal; i++) {
        int found = 0;
        for (int j = 0; j < nnext; j++) {
            if (strcmp(g_real[i].id, next[j].id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            emit_gone(g_real[i].id);
        }
    }

    /* Bước 3: plug mới hoặc đổi payload → emit; kế thừa claim_fd theo id. */
    for (int j = 0; j < nnext; j++) {
        ud_dev *old = NULL;
        for (int i = 0; i < g_nreal; i++) {
            if (strcmp(g_real[i].id, next[j].id) == 0) {
                old = &g_real[i];
                next[j].claim_fd = old->claim_fd;
                break;
            }
        }
        if (!old || !same_payload(old, &next[j])) {
            emit_dev(&next[j]);
        }
    }

    memcpy(g_real, next, sizeof next);
    g_nreal = nnext;
}

/* Peer disconnect → nhả mọi claim đang giữ bởi fd đó. */
static void drop_claims(int fd)
{
    for (int i = 0; i < g_nreal; i++) {
        if (g_real[i].claim_fd == fd) {
            g_real[i].claim_fd = -1;
        }
    }
    for (int i = 0; i < g_nsim; i++) {
        if (g_sim[i].claim_fd == fd) {
            g_sim[i].claim_fd = -1;
        }
    }
}

static void close_peer(int index)
{
    if (g_peers[index].fd >= 0) {
        drop_claims(g_peers[index].fd); /* tránh claim treo sau khi client chết */
        close(g_peers[index].fd);
    }
    g_peers[index].fd = -1;
    g_peers[index].len = 0;
    g_peers[index].sub = 0;
}

static int sim_index(const char *id)
{
    for (int i = 0; i < g_nsim; i++) {
        if (strcmp(g_sim[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

static void sim_remove_at(int index)
{
    emit_gone(g_sim[index].id);
    g_sim[index] = g_sim[g_nsim - 1];
    g_nsim--;
}

static void sim_remove_id(const char *id)
{
    int index = sim_index(id);
    if (index >= 0) {
        sim_remove_at(index);
    }
}

static void sim_clear(void)
{
    while (g_nsim > 0) {
        sim_remove_at(g_nsim - 1);
    }
}

/* Thêm/ghi đè sim cùng id rồi broadcast như plug thật. */
static void sim_add(ud_dev dev)
{
    int index;
    sim_remove_id(dev.id); /* thay thế nếu đã có */
    if (g_nsim >= (int)(sizeof g_sim / sizeof g_sim[0])) {
        return;
    }
    dev.claim_fd = -1;
    dev.live = 1;
    index = g_nsim++;
    g_sim[index] = dev;
    emit_dev(&g_sim[index]);
}

/* Google ADB mẫu (vid 0x18d1) — usb-man sẽ classify ANDROID → chạy AOA. */
static void fill_android(ud_dev *d)
{
    memset(d, 0, sizeof *d);
    snprintf(d->id, sizeof d->id, "sim-android");
    d->vid = 0x18d1;
    d->pid = 0x4ee2;
    snprintf(d->serial, sizeof d->serial, "SIM-ANDROID");
    snprintf(d->mfg, sizeof d->mfg, "Android");
    snprintf(d->prod, sizeof d->prod, "Phone");
    snprintf(d->ifaces, sizeof d->ifaces, "ff/42/01"); /* ADB class/subclass/proto */
    d->adb = 1;
    d->sim = 1;
    d->claim_fd = -1;
}

/* iPhone + CDC-NCM mẫu — usb-man classify CARPLAY → link up + media. */
static void fill_carplay(ud_dev *d)
{
    memset(d, 0, sizeof *d);
    snprintf(d->id, sizeof d->id, "sim-carplay");
    d->vid = 0x05ac;
    d->pid = 0x12a8;
    snprintf(d->serial, sizeof d->serial, "SIM-CARPLAY");
    snprintf(d->mfg, sizeof d->mfg, "Apple");
    snprintf(d->prod, sizeof d->prod, "iPhone");
    snprintf(d->ifaces, sizeof d->ifaces, "02/0d/00"); /* CDC-NCM */
    snprintf(d->drivers, sizeof d->drivers, "cdc_ncm");
    snprintf(d->net, sizeof d->net, "usb0");
    d->ncm = 1;
    d->apple = 1;
    d->sim = 1;
    d->claim_fd = -1;
}

static int reply(int fd, const char *line)
{
    return hupi_send_line(fd, line);
}

/*
 * Control transfer: "ctrl <id> bm req wValue wIndex wLength [hexdata]"
 * Chỉ peer đang claim mới được gọi. Thiết bị sim giả lập AOA:
 *   req 51 (GET_PROTOCOL) → trả protocol=2
 *   req 52 (SEND_STRING)  → ok
 *   req 53 (START)        → đổi sang AOAP (pid 0x2d00) và re-emit device
 */
static void handle_ctrl(int fd, ud_dev *dev, char *args)
{
    unsigned bm = 0, req = 0, val = 0, idx = 0, wlen = 0;
    char hex[600] = "";
    uint8_t data[256];
    size_t nbytes = 0;
    int in_dir;
    int got;

    /* Exclusive: chỉ peer đang claim mới được gửi control. */
    if (dev->claim_fd != fd) {
        reply(fd, "err need-claim");
        return;
    }
    got = sscanf(args, "%u %u %u %u %u %599s", &bm, &req, &val, &idx, &wlen, hex);
    if (got < 5 || wlen > sizeof data) {
        reply(fd, "err bad-ctrl");
        return;
    }
    if (got == 6 && hupi_hex_decode(hex, data, sizeof data, &nbytes) != 0) {
        reply(fd, "err bad-hex");
        return;
    }
    in_dir = (bm & 0x80) != 0; /* bit7 bmRequestType: 1=IN (device→host), 0=OUT */
    if (!in_dir && nbytes != wlen && wlen != 0) {
        if (nbytes == 0 && wlen == 0) {
            /* OUT không payload — hợp lệ với AOA START (req 53). */
        } else if (nbytes != wlen) {
            reply(fd, "err length");
            return;
        }
    }

    /* ---- Nhánh sim: giả lập AOA không cần kernel/usbfs ---- */
    if (dev->sim) {
        char enc[600];
        if (in_dir && req == 51) {
            /* GET_PROTOCOL → LE uint16 = 2 (AOA v2). */
            data[0] = 2;
            data[1] = 0;
            hupi_hex_encode(data, 2, enc, sizeof enc);
            snprintf(hex, sizeof hex, "ok ctrl %s", enc);
            reply(fd, hex);
            return;
        }
        if (!in_dir && req == 52) {
            reply(fd, "ok ctrl"); /* SEND_STRING: nhận chuỗi, không đổi state */
            return;
        }
        if (!in_dir && req == 53) {
            /* START: mô phỏng phone re-enum sang AOAP cùng id. */
            dev->pid = 0x2d00;
            dev->adb = 0;
            dev->accessory = 1;
            snprintf(dev->ifaces, sizeof dev->ifaces, "ff/ff/00");
            snprintf(dev->prod, sizeof dev->prod, "AOAP");
            emit_gone(dev->id); /* usb-man thấy unplug (pending_reenum) */
            emit_dev(dev);      /* rồi plug lại dạng AOAP */
            reply(fd, "ok ctrl");
            return;
        }
        reply(fd, "err sim-ctrl");
        return;
    }

    /* ---- Nhánh thiết bị thật: mở usbfs, ioctl USBDEVFS_CONTROL ---- */
    {
        usbdrv_control_t setup;
        int usbfd;
        int rc;
        char enc[600];
        char line[700];

        memset(&setup, 0, sizeof setup);
        setup.bm_request_type = (uint8_t)bm;
        setup.b_request = (uint8_t)req;
        setup.w_value = (uint16_t)val;
        setup.w_index = (uint16_t)idx;
        setup.w_length = (uint16_t)wlen;
        usbfd = usbdrv_usbfs_open(dev->node);
        if (usbfd < 0) {
            reply(fd, "err usbfs-open");
            return;
        }
        if (in_dir) {
            memset(data, 0, wlen); /* buffer nhận từ device */
        }
        rc = usbdrv_usbfs_control(usbfd, &setup, data, 1000);
        close(usbfd);
        if (rc < 0) {
            reply(fd, "err usbfs");
            return;
        }
        if (in_dir) {
            /* Trả data IN dưới dạng hex để usb-man parse (vd. protocol). */
            hupi_hex_encode(data, (size_t)rc, enc, sizeof enc);
            snprintf(line, sizeof line, "ok ctrl %s", enc);
            reply(fd, line);
        } else {
            reply(fd, "ok ctrl");
        }
    }
}

/*
 * Lệnh client trên usb-driver.sock:
 *   sub | list | claim/release <id> | ctrl ... | sim plug/unplug ...
 */
static void handle_line(int peer_index, const char *line)
{
    int fd = g_peers[peer_index].fd;
    ud_dev *dev;

    if (strcmp(line, "sub") == 0) {
        /* Đăng ký nhận broadcast + dump snapshot hiện tại (catch-up). */
        g_peers[peer_index].sub = 1;
        for (int i = 0; i < g_nreal; i++) {
            char dump[1400];
            format_dev(&g_real[i], dump, sizeof dump);
            hupi_send_line(fd, dump);
        }
        for (int i = 0; i < g_nsim; i++) {
            char dump[1400];
            format_dev(&g_sim[i], dump, sizeof dump);
            hupi_send_line(fd, dump);
        }
        reply(fd, "ok");
        return;
    }
    if (strcmp(line, "list") == 0) {
        /* Một lần dump, không đăng ký broadcast. */
        for (int i = 0; i < g_nreal; i++) {
            char dump[1400];
            format_dev(&g_real[i], dump, sizeof dump);
            hupi_send_line(fd, dump);
        }
        for (int i = 0; i < g_nsim; i++) {
            char dump[1400];
            format_dev(&g_sim[i], dump, sizeof dump);
            hupi_send_line(fd, dump);
        }
        reply(fd, "ok");
        return;
    }
    if (strcmp(line, "sim plug android") == 0) {
        ud_dev d;
        HUPI_LOGI("sim plug android");
        fill_android(&d);
        sim_add(d);
        reply(fd, "ok");
        return;
    }
    if (strcmp(line, "sim plug carplay") == 0) {
        ud_dev d;
        HUPI_LOGI("sim plug carplay");
        fill_carplay(&d);
        sim_add(d);
        reply(fd, "ok");
        return;
    }
    if (strcmp(line, "sim unplug") == 0) {
        HUPI_LOGI("sim unplug all");
        sim_clear();
        reply(fd, "ok");
        return;
    }
    if (strncmp(line, "sim unplug ", 11) == 0) {
        sim_remove_id(line + 11); /* rút một id cụ thể */
        reply(fd, "ok");
        return;
    }
    if (strncmp(line, "claim ", 6) == 0) {
        /* Exclusive lease trước khi ctrl — tránh hai process AOA cùng lúc. */
        dev = find_id(line + 6);
        if (!dev) {
            reply(fd, "err no-device");
            return;
        }
        if (dev->claim_fd >= 0 && dev->claim_fd != fd) {
            reply(fd, "err busy");
            return;
        }
        dev->claim_fd = fd;
        HUPI_LOGI("claim id=%s peer=%d", dev->id, fd);
        reply(fd, "ok");
        return;
    }
    if (strncmp(line, "release ", 8) == 0) {
        dev = find_id(line + 8);
        if (!dev) {
            reply(fd, "err no-device");
            return;
        }
        if (dev->claim_fd == fd) {
            dev->claim_fd = -1;
        }
        reply(fd, "ok");
        return;
    }
    if (strncmp(line, "ctrl ", 5) == 0) {
        /* "ctrl <id> <bm> <req> ..." — tách id rồi giao handle_ctrl. */
        char id[80];
        const char *rest;
        if (sscanf(line + 5, "%79s", id) != 1) {
            reply(fd, "err bad-ctrl");
            return;
        }
        rest = line + 5 + strlen(id);
        while (*rest == ' ') {
            rest++;
        }
        dev = find_id(id);
        if (!dev) {
            reply(fd, "err no-device");
            return;
        }
        handle_ctrl(fd, dev, (char *)rest);
        return;
    }
    reply(fd, "err unknown");
}

static void accept_peer(void)
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
    close(fd); /* đầy slot → từ chối kết nối mới */
}

/* Đọc bytes từ peer, tách từng dòng hoàn chỉnh rồi dispatch. */
static void drain_peer(int index)
{
    char line[1400];
    if (hupi_peer_recv(&g_peers[index]) != 0) {
        close_peer(index); /* EOF / lỗi socket */
        return;
    }
    for (;;) {
        int rc = hupi_peer_pull(&g_peers[index], line, sizeof line);
        if (rc == 0) {
            break; /* chưa đủ một dòng */
        }
        if (rc < 0) {
            close_peer(index); /* framing lỗi (buffer đầy) */
            break;
        }
        handle_line(index, line);
    }
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

    /* --log-level error|warn|info|trace ; --runtime DIR */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--log-level") && i + 1 < argc) {
            hupi_log_level_from_str(argv[i + 1], &level);
        }
    }
    hupi_log_open("usb-driver", level);

    for (int i = 0; i < MAX_PEER; i++) {
        g_peers[i].fd = -1;
    }
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (hupi_mkdir_runtime(runtime) != 0) {
        fprintf(stderr, "usb-driverd: cannot create %s\n", runtime);
        return 1;
    }
    hupi_sock_path(g_sock, sizeof g_sock, runtime, HUPI_DRIVER_SOCK);
    g_listen = hupi_listen_unix(g_sock);
    if (g_listen < 0) {
        fprintf(stderr, "usb-driverd: listen %s failed\n", g_sock);
        return 1;
    }
    hupi_set_nonblock(g_listen);
    g_uevent = usbdrv_uevent_open();
    if (g_uevent >= 0) {
        hupi_set_nonblock(g_uevent);
    } else {
        HUPI_LOGW("netlink uevent unavailable; sim and initial scan still work");
    }
    refresh_real(); /* quét ban đầu trước khi chờ uevent */
    HUPI_LOGI("listen %s", g_sock);

    /* Vòng poll: accept client | uevent USB → refresh | lệnh từ peer */
    while (!g_stop) {
        struct pollfd pfds[2 + MAX_PEER];
        int np = 0;
        int map[2 + MAX_PEER]; /* -1=listen, -2=uevent, >=0 = peer index */

        pfds[np].fd = g_listen;
        pfds[np].events = POLLIN;
        map[np] = -1;
        np++;
        if (g_uevent >= 0) {
            pfds[np].fd = g_uevent;
            pfds[np].events = POLLIN;
            map[np] = -2;
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
        if (poll(pfds, (nfds_t)np, 500) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        for (int i = 0; i < np; i++) {
            if (!(pfds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }
            if (map[i] == -1) {
                accept_peer();
            } else if (map[i] == -2) {
                /* Uevent chỉ là tín hiệu; luôn quét lại sysfs để có state đúng. */
                usbdrv_uevent_t ev;
                int rc = usbdrv_uevent_recv(g_uevent, &ev);
                if (rc > 0 && strcmp(ev.subsystem, "usb") == 0) {
                    refresh_real();
                }
            } else {
                drain_peer(map[i]);
            }
        }
    }
    unlink(g_sock); /* dọn socket file khi thoát */
    return 0;
}
