/**
 * @file usb-panel/main.c
 * @brief Demo UI điều khiển: gửi lệnh sim/stream tới usb-managerd.
 */
#include "ui.h"

#include "hupi_wire.h"

#include <stdio.h>
#include <string.h>

enum { W = 520, H = 720 };

typedef struct {
    SDL_Rect rect;
    const char *cmd;
    const char *text;
    int r, g, b;
} panel_button;

static SDL_Color rgb(int r, int g, int b)
{
    SDL_Color c = {(Uint8)r, (Uint8)g, (Uint8)b, 255};
    return c;
}

static void fill(SDL_Renderer *renderer, SDL_Rect rc, SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(renderer, &rc);
}

int main(int argc, char **argv)
{
    const char *runtime = hupi_runtime(argc, argv);
    SDL_Window *window;
    SDL_Renderer *renderer;
    hupi_font *font;
    hupi_font *small;
    hupi_link link;
    hupi_label title, hint, state_l, detail_l, labels[5];
    char state_txt[240] = "Chưa nối tới usb-manager";
    char detail[240] = "Bấm một nút để giả lập cắm hoặc rút USB.";
    int running = 1;
    panel_button buttons[5] = {
        {{36, 150, 448, 72}, "sim plug android", "Cắm Android (AOA)", 22, 130, 74},
        {{36, 238, 448, 72}, "sim plug carplay", "Cắm iPhone (CarPlay)", 20, 96, 170},
        {{36, 326, 448, 72}, "sim unplug", "Rút USB", 180, 70, 55},
        {{36, 430, 214, 64}, "stream on", "Bật stream", 15, 118, 110},
        {{270, 430, 214, 64}, "stream off", "Tắt stream", 71, 85, 105},
    };

    memset(&link, 0, sizeof link);
    link.fd = -1;
    memset(&title, 0, sizeof title);
    memset(&hint, 0, sizeof hint);
    memset(&state_l, 0, sizeof state_l);
    memset(&detail_l, 0, sizeof detail_l);
    memset(labels, 0, sizeof labels);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("HUPI — giả lập USB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, W, H);
    font = hupi_font_open(22);
    small = hupi_font_open(18);
    if (!font || !small) {
        fprintf(stderr, "Không mở được font DejaVuSans\n");
        return 1;
    }

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                float x, y;
                SDL_RenderWindowToLogical(renderer, ev.button.x, ev.button.y, &x, &y);
                for (int i = 0; i < 5; i++) {
                    SDL_Rect r = buttons[i].rect;
                    if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) {
                        if (link.fd < 0) {
                            snprintf(detail, sizeof detail, "usb-manager chưa chạy");
                        } else if (hupi_link_send(&link, buttons[i].cmd) != 0) {
                            snprintf(detail, sizeof detail, "mất kết nối, đang thử lại");
                        } else {
                            snprintf(detail, sizeof detail, "đã gửi: %s", buttons[i].cmd);
                        }
                    }
                }
            }
        }
        if (link.fd < 0 && hupi_link_open(&link, runtime, HUPI_MANAGER_SOCK) == 0) {
            hupi_link_send(&link, "sub");
            snprintf(detail, sizeof detail, "đã nối %s", runtime);
        }
        if (link.fd >= 0) {
            char line[512];
            while (hupi_link_poll(&link, line, sizeof line) == 1) {
                if (strncmp(line, "state ", 6) == 0) {
                    char backend[32], phase[32], device[80], reason[80];
                    int streaming = 0;
                    hupi_kv_get(line, "backend", backend, sizeof backend);
                    hupi_kv_get(line, "phase", phase, sizeof phase);
                    hupi_kv_get(line, "device", device, sizeof device);
                    hupi_kv_get(line, "reason", reason, sizeof reason);
                    hupi_kv_get_int(line, "streaming", &streaming);
                    snprintf(state_txt, sizeof state_txt, "%s / %s", backend, phase);
                    snprintf(detail, sizeof detail, "thiết bị %s · %s · stream %s", device, reason,
                             streaming ? "bật" : "tắt");
                } else if (strncmp(line, "err", 3) == 0) {
                    snprintf(detail, sizeof detail, "%s", line);
                }
            }
        }

        {
            SDL_Rect bg = {0, 0, W, H};
            fill(renderer, bg, rgb(244, 247, 250));
        }
        {
            SDL_Rect head = {0, 0, W, 96};
            fill(renderer, head, rgb(28, 42, 58));
        }
        hupi_label_set(&title, renderer, font, rgb(255, 255, 255), "Giả lập tính năng USB");
        hupi_label_draw(&title, renderer, 28, 22);
        hupi_label_set(&hint, renderer, small, rgb(186, 210, 230), "plugin / unplug không cần điện thoại");
        hupi_label_draw(&hint, renderer, 28, 56);

        for (int i = 0; i < 5; i++) {
            SDL_Color fg = rgb(255, 255, 255);
            fill(renderer, buttons[i].rect, rgb(buttons[i].r, buttons[i].g, buttons[i].b));
            hupi_label_set(&labels[i], renderer, font, fg, buttons[i].text);
            hupi_label_draw(&labels[i], renderer, buttons[i].rect.x + 18, buttons[i].rect.y + 22);
        }
        {
            SDL_Rect box = {36, 520, 448, 164};
            fill(renderer, box, rgb(255, 255, 255));
        }
        hupi_label_set(&state_l, renderer, font, rgb(28, 42, 58), state_txt);
        hupi_label_draw(&state_l, renderer, 52, 544);
        hupi_label_set(&detail_l, renderer, small, rgb(71, 85, 105), detail);
        hupi_label_draw(&detail_l, renderer, 52, 590);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    hupi_link_close(&link);
    hupi_label_free(&title);
    hupi_label_free(&hint);
    hupi_label_free(&state_l);
    hupi_label_free(&detail_l);
    for (int i = 0; i < 5; i++) {
        hupi_label_free(&labels[i]);
    }
    hupi_font_close(font);
    hupi_font_close(small);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
