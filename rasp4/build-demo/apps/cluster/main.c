/**
 * @file cluster/main.c
 * @brief Demo cluster: hiển thị state session + nhận frame từ usb-stream.sock.
 */
#include "ui.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { W = 1280, H = 720 };

static SDL_Color rgb(int r, int g, int b)
{
    SDL_Color c = {(Uint8)r, (Uint8)g, (Uint8)b, 255};
    return c;
}

static void fill(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(r, &rc);
}

static void card(SDL_Renderer *r, int x, int y, int w, int h)
{
    fill(r, x, y, w, h, rgb(255, 255, 255));
}

static int g_stream = -1;
static uint8_t g_acc[20 + HUPI_FRAME_W * HUPI_FRAME_H * 3];
static size_t g_have;
static SDL_Texture *g_frame;
static int g_video_is_h264;
static uint32_t g_h264_bytes;
static uint32_t g_h264_frames;

static void take_stream(SDL_Renderer *renderer, const char *runtime)
{
    char path[256];
    if (g_stream < 0) {
        hupi_sock_path(path, sizeof path, runtime, HUPI_STREAM_SOCK);
        g_stream = hupi_connect_unix(path);
        if (g_stream >= 0) {
            hupi_set_nonblock(g_stream);
            g_have = 0;
        }
        return;
    }
    while (g_have < sizeof g_acc) {
        ssize_t n = recv(g_stream, g_acc + g_have, sizeof g_acc - g_have, 0);
        uint32_t magic, stride, nbytes, fw, fh;
        int is_h264;
        if (n == 0) {
            close(g_stream);
            g_stream = -1;
            g_have = 0;
            return;
        }
        if (n < 0) {
            break;
        }
        g_have += (size_t)n;
        if (g_have < 20) {
            continue;
        }
        magic = (uint32_t)g_acc[0] | ((uint32_t)g_acc[1] << 8) | ((uint32_t)g_acc[2] << 16) |
                ((uint32_t)g_acc[3] << 24);
        fw = (uint32_t)g_acc[4] | ((uint32_t)g_acc[5] << 8) | ((uint32_t)g_acc[6] << 16) |
             ((uint32_t)g_acc[7] << 24);
        fh = (uint32_t)g_acc[8] | ((uint32_t)g_acc[9] << 8) | ((uint32_t)g_acc[10] << 16) |
             ((uint32_t)g_acc[11] << 24);
        stride = (uint32_t)g_acc[12] | ((uint32_t)g_acc[13] << 8) | ((uint32_t)g_acc[14] << 16) |
                 ((uint32_t)g_acc[15] << 24);
        nbytes = (uint32_t)g_acc[16] | ((uint32_t)g_acc[17] << 8) | ((uint32_t)g_acc[18] << 16) |
                 ((uint32_t)g_acc[19] << 24);
        is_h264 = (magic == HUPI_H264_MAGIC);
        if ((!is_h264 && magic != HUPI_FRAME_MAGIC) || fw != HUPI_FRAME_W || fh != HUPI_FRAME_H ||
            nbytes > sizeof g_acc - 20) {
            memmove(g_acc, g_acc + 1, g_have - 1);
            g_have--;
            continue;
        }
        if (g_have < 20u + nbytes) {
            break;
        }
        if (is_h264) {
            /* Chưa decode H.264 trong demo SDL — chỉ đếm AU để xác nhận pipeline. */
            g_video_is_h264 = 1;
            g_h264_bytes = nbytes;
            g_h264_frames++;
        } else {
            g_video_is_h264 = 0;
            if (!g_frame) {
                g_frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                            (int)fw, (int)fh);
            }
            if (g_frame) {
                SDL_UpdateTexture(g_frame, NULL, g_acc + 20, (int)stride);
            }
        }
        memmove(g_acc, g_acc + 20 + nbytes, g_have - 20 - nbytes);
        g_have -= 20u + nbytes;
    }
}

int main(int argc, char **argv)
{
    const char *runtime = hupi_runtime(argc, argv);
    int fullscreen = 0;
    SDL_Window *window;
    SDL_Renderer *renderer;
    hupi_font *font;
    hupi_font *font_lg;
    hupi_link link;
    Uint32 flags = SDL_WINDOW_RESIZABLE;
    int running = 1;
    char backend[32] = "none";
    char phase[32] = "idle";
    char device[80] = "-";
    char reason[96] = "-";
    char net[32] = "-";
    int streaming = 0;
    int touchq = 0;
    char status[192] = "Đang chờ usb-manager";
    hupi_label title, sub, left_h, app1, app2, app3, right_h, speed, speed_u, center, foot, badge;
    SDL_Rect proj = {270, 92, 700, 500};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = 1;
        }
    }
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    memset(&link, 0, sizeof link);
    link.fd = -1;
    memset(&title, 0, sizeof title);
    memset(&sub, 0, sizeof sub);
    memset(&left_h, 0, sizeof left_h);
    memset(&app1, 0, sizeof app1);
    memset(&app2, 0, sizeof app2);
    memset(&app3, 0, sizeof app3);
    memset(&right_h, 0, sizeof right_h);
    memset(&speed, 0, sizeof speed);
    memset(&speed_u, 0, sizeof speed_u);
    memset(&center, 0, sizeof center);
    memset(&foot, 0, sizeof foot);
    memset(&badge, 0, sizeof badge);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("HUPI — màn hình giả lập", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W,
                              H, flags);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, W, H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    font = hupi_font_open(22);
    font_lg = hupi_font_open(54);
    if (!font || !font_lg) {
        fprintf(stderr, "Không mở được font DejaVuSans\n");
        return 1;
    }

    while (running) {
        SDL_Event ev;
        int speed_km = streaming ? (int)((SDL_GetTicks() / 80) % 40) + 48 : 0;
        char speed_txt[16];
        char center_txt[160];
        char foot_txt[220];
        SDL_Color accent = rgb(71, 85, 105);

        while (SDL_PollEvent(&ev)) {
            float lx, ly;
            if (ev.type == SDL_QUIT) {
                running = 0;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || (ev.type == SDL_MOUSEMOTION && (ev.motion.state & SDL_BUTTON_LMASK))) {
                int x = ev.type == SDL_MOUSEBUTTONDOWN ? ev.button.x : ev.motion.x;
                int y = ev.type == SDL_MOUSEBUTTONDOWN ? ev.button.y : ev.motion.y;
                int down = ev.type == SDL_MOUSEBUTTONDOWN ? 1 : 1;
                SDL_RenderWindowToLogical(renderer, x, y, &lx, &ly);
                if (lx >= proj.x && ly >= proj.y && lx < proj.x + proj.w && ly < proj.y + proj.h) {
                    char cmd[64];
                    int nx = (int)((lx - proj.x) * 10000 / proj.w);
                    int ny = (int)((ly - proj.y) * 10000 / proj.h);
                    if (ev.type == SDL_MOUSEBUTTONUP) {
                        down = 0;
                    }
                    snprintf(cmd, sizeof cmd, "touch %d %d %d", nx, ny, down);
                    if (link.fd >= 0) {
                        hupi_link_send(&link, cmd);
                    }
                }
            }
            if (ev.type == SDL_MOUSEBUTTONUP) {
                float lx2, ly2;
                SDL_RenderWindowToLogical(renderer, ev.button.x, ev.button.y, &lx2, &ly2);
                if (lx2 >= proj.x && ly2 >= proj.y && lx2 < proj.x + proj.w && ly2 < proj.y + proj.h &&
                    link.fd >= 0) {
                    char cmd[64];
                    int nx = (int)((lx2 - proj.x) * 10000 / proj.w);
                    int ny = (int)((ly2 - proj.y) * 10000 / proj.h);
                    snprintf(cmd, sizeof cmd, "touch %d %d 0", nx, ny);
                    hupi_link_send(&link, cmd);
                }
            }
        }

        if (link.fd < 0) {
            if (hupi_link_open(&link, runtime, HUPI_MANAGER_SOCK) == 0) {
                hupi_link_send(&link, "sub");
                link.subscribed = 1;
            }
        }
        if (link.fd >= 0) {
            char line[512];
            int rc;
            while ((rc = hupi_link_poll(&link, line, sizeof line)) == 1) {
                if (strncmp(line, "state ", 6) == 0) {
                    hupi_kv_get(line, "backend", backend, sizeof backend);
                    hupi_kv_get(line, "phase", phase, sizeof phase);
                    hupi_kv_get(line, "device", device, sizeof device);
                    hupi_kv_get(line, "reason", reason, sizeof reason);
                    hupi_kv_get(line, "net", net, sizeof net);
                    hupi_kv_get_int(line, "streaming", &streaming);
                    hupi_kv_get_int(line, "touchq", &touchq);
                    snprintf(status, sizeof status, "%s · %s", backend, phase);
                }
            }
        }
        if (streaming) {
            take_stream(renderer, runtime);
        }

        if (strcmp(backend, "android") == 0) {
            accent = rgb(22, 130, 74);
            if (g_video_is_h264) {
                snprintf(center_txt, sizeof center_txt, "Android Auto · H.264 stub (%u B · #%u)",
                         g_h264_bytes, g_h264_frames);
            } else {
                snprintf(center_txt, sizeof center_txt, "Android Auto · AOA");
            }
        } else if (strcmp(backend, "carplay") == 0) {
            accent = rgb(20, 96, 170);
            if (g_video_is_h264) {
                snprintf(center_txt, sizeof center_txt, "CarPlay · H.264 stub (%u B · #%u)",
                         g_h264_bytes, g_h264_frames);
            } else {
                snprintf(center_txt, sizeof center_txt, "CarPlay · CDC-NCM · IPv6");
            }
        } else {
            accent = rgb(71, 85, 105);
            snprintf(center_txt, sizeof center_txt, "Chưa có phiên USB");
        }
        snprintf(speed_txt, sizeof speed_txt, "%d", speed_km);
        snprintf(foot_txt, sizeof foot_txt, "thiết bị %s  ·  %s  ·  net %s  ·  touch %d", device, reason, net,
                 touchq);

        fill(renderer, 0, 0, W, H, rgb(232, 238, 242));
        fill(renderer, 0, 0, W, 64, rgb(28, 42, 58));
        hupi_label_set(&title, renderer, font, rgb(255, 255, 255), "HUPI  ·  màn hình giả lập");
        hupi_label_draw(&title, renderer, 24, 18);
        hupi_label_set(&sub, renderer, font, rgb(186, 210, 230), status);
        hupi_label_draw(&sub, renderer, 860, 18);

        card(renderer, 16, 80, 240, 624);
        hupi_label_set(&left_h, renderer, font, rgb(28, 42, 58), "Ứng dụng");
        hupi_label_draw(&left_h, renderer, 36, 100);
        fill(renderer, 32, 150, 208, 64, rgb(241, 245, 249));
        fill(renderer, 32, 230, 208, 64, rgb(241, 245, 249));
        fill(renderer, 32, 310, 208, 64, rgb(241, 245, 249));
        hupi_label_set(&app1, renderer, font, rgb(30, 41, 59), "Điều hướng");
        hupi_label_set(&app2, renderer, font, rgb(30, 41, 59), "Điện thoại");
        hupi_label_set(&app3, renderer, font, rgb(30, 41, 59), "Nhạc");
        hupi_label_draw(&app1, renderer, 52, 168);
        hupi_label_draw(&app2, renderer, 52, 248);
        hupi_label_draw(&app3, renderer, 52, 328);

        card(renderer, proj.x - 8, 80, proj.w + 16, 624);
        fill(renderer, proj.x, proj.y, proj.w, proj.h, rgb(15, 23, 42));
        if (g_frame && streaming && !g_video_is_h264) {
            SDL_RenderCopy(renderer, g_frame, NULL, &proj);
        } else if (streaming && g_video_is_h264) {
            /* Placeholder khi nhận H.264 — decode thật sẽ do hu-graphics / GStreamer. */
            fill(renderer, proj.x + 40, proj.y + 120, proj.w - 80, proj.h - 200, rgb(30, 41, 59));
        }
        hupi_label_set(&center, renderer, font, rgb(255, 255, 255), center_txt);
        hupi_label_draw(&center, renderer, proj.x + 24, proj.y + 16);
        hupi_label_set(&badge, renderer, font, accent,
                       streaming ? (g_video_is_h264 ? "H264" : "STREAM") : "CHỜ");
        hupi_label_draw(&badge, renderer, proj.x + proj.w - badge.w - 24, proj.y + 16);
        hupi_label_set(&foot, renderer, font, rgb(51, 65, 85), foot_txt);
        hupi_label_draw(&foot, renderer, proj.x, 600);

        card(renderer, 1000, 80, 264, 624);
        hupi_label_set(&right_h, renderer, font, rgb(28, 42, 58), "Đồng hồ");
        hupi_label_draw(&right_h, renderer, 1020, 100);
        fill(renderer, 1048, 180, 168, 168, rgb(241, 245, 249));
        hupi_label_set(&speed, renderer, font_lg, accent, speed_txt);
        hupi_label_draw(&speed, renderer, 1048 + (168 - speed.w) / 2, 210);
        hupi_label_set(&speed_u, renderer, font, rgb(71, 85, 105), "km/h");
        hupi_label_draw(&speed_u, renderer, 1048 + (168 - speed_u.w) / 2, 290);

        SDL_RenderPresent(renderer);
        if (!renderer) {
            break;
        }
        SDL_Delay(16);
    }

    hupi_link_close(&link);
    if (g_stream >= 0) {
        close(g_stream);
    }
    hupi_label_free(&title);
    hupi_label_free(&sub);
    hupi_label_free(&left_h);
    hupi_label_free(&app1);
    hupi_label_free(&app2);
    hupi_label_free(&app3);
    hupi_label_free(&right_h);
    hupi_label_free(&speed);
    hupi_label_free(&speed_u);
    hupi_label_free(&center);
    hupi_label_free(&foot);
    hupi_label_free(&badge);
    hupi_font_close(font);
    hupi_font_close(font_lg);
    if (g_frame) {
        SDL_DestroyTexture(g_frame);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
