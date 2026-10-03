#include "ui.h"

#include "hupi_wire.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct hupi_font {
    unsigned char *data;
    stbtt_fontinfo info;
    float scale;
    int ascent;
};

static const char *k_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/ttf/LiberationSans-Regular.ttf",
    NULL,
};

static unsigned char *read_all(const char *path, size_t *n)
{
    FILE *fp = fopen(path, "rb");
    unsigned char *buf;
    long len;
    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    len = ftell(fp);
    if (len <= 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    buf = malloc((size_t)len);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *n = (size_t)len;
    return buf;
}

hupi_font *hupi_font_open(float px)
{
    hupi_font *font = calloc(1, sizeof *font);
    size_t n = 0;
    if (!font) {
        return NULL;
    }
    for (int i = 0; k_fonts[i]; i++) {
        font->data = read_all(k_fonts[i], &n);
        if (font->data) {
            break;
        }
    }
    if (!font->data || !stbtt_InitFont(&font->info, font->data, 0)) {
        free(font->data);
        free(font);
        return NULL;
    }
    font->scale = stbtt_ScaleForPixelHeight(&font->info, px);
    {
        int ascent = 0, descent = 0, gap = 0;
        stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &gap);
        font->ascent = (int)(ascent * font->scale);
    }
    return font;
}

void hupi_font_close(hupi_font *font)
{
    if (!font) {
        return;
    }
    free(font->data);
    free(font);
}

static unsigned utf8_next(const char **text)
{
    const unsigned char *s = (const unsigned char *)*text;
    unsigned cp;
    if (*s == 0) {
        return 0;
    }
    if (*s < 0x80) {
        *text += 1;
        return *s;
    }
    if ((*s & 0xe0) == 0xc0 && (s[1] & 0xc0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1f) << 6) | (s[1] & 0x3f);
        *text += 2;
        return cp;
    }
    if ((*s & 0xf0) == 0xe0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0f) << 12) | ((unsigned)(s[1] & 0x3f) << 6) | (s[2] & 0x3f);
        *text += 3;
        return cp;
    }
    if ((*s & 0xf8) == 0xf0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 && (s[3] & 0xc0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3f) << 12) |
             ((unsigned)(s[2] & 0x3f) << 6) | (s[3] & 0x3f);
        *text += 4;
        return cp;
    }
    *text += 1;
    return 0xfffd;
}

static int measure(hupi_font *font, const char *text)
{
    const char *p = text;
    int width = 0;
    int advance = 0;
    int lsb = 0;
    while (*p) {
        unsigned cp = utf8_next(&p);
        stbtt_GetCodepointHMetrics(&font->info, (int)cp, &advance, &lsb);
        width += (int)(advance * font->scale);
    }
    return width > 0 ? width : 1;
}

void hupi_label_set(hupi_label *label, SDL_Renderer *renderer, hupi_font *font, SDL_Color color,
                    const char *text)
{
    int width;
    int height;
    SDL_Surface *surface;
    uint32_t *px;
    const char *p;
    int pen = 0;

    if (!label || !font || !text) {
        return;
    }
    if (label->tex && strcmp(label->text, text) == 0 && label->color.r == color.r &&
        label->color.g == color.g && label->color.b == color.b) {
        return;
    }
    hupi_label_free(label);
    snprintf(label->text, sizeof label->text, "%s", text);
    label->color = color;
    width = measure(font, text) + 2;
    height = font->ascent + (int)(6 * font->scale) + 4;
    if (height < 8) {
        height = 8;
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }
    px = surface->pixels;
    p = text;
    while (*p) {
        const char *before = p;
        unsigned cp = utf8_next(&p);
        int advance = 0, lsb = 0, gw = 0, gh = 0, xoff = 0, yoff = 0;
        unsigned char *glyph;
        int x0, y0, x1, y1;
        (void)before;
        stbtt_GetCodepointHMetrics(&font->info, (int)cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&font->info, (int)cp, font->scale, font->scale, &x0, &y0, &x1, &y1);
        glyph = stbtt_GetCodepointBitmap(&font->info, 0, font->scale, (int)cp, &gw, &gh, &xoff, &yoff);
        if (glyph) {
            int top = font->ascent + y0;
            for (int gy = 0; gy < gh; gy++) {
                int dy = top + gy;
                if (dy < 0 || dy >= height) {
                    continue;
                }
                for (int gx = 0; gx < gw; gx++) {
                    int dx = pen + x0 + gx;
                    unsigned char a = glyph[gy * gw + gx];
                    uint32_t *dst;
                    if (dx < 0 || dx >= width || a == 0) {
                        continue;
                    }
                    dst = px + dy * (surface->pitch / 4) + dx;
                    *dst = ((uint32_t)a << 24) | ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) |
                           color.b;
                }
            }
            stbtt_FreeBitmap(glyph, NULL);
        }
        pen += (int)(advance * font->scale);
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
    label->tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (label->tex) {
        SDL_SetTextureBlendMode(label->tex, SDL_BLENDMODE_BLEND);
        label->w = width;
        label->h = height;
    }
    SDL_FreeSurface(surface);
}

void hupi_label_draw(hupi_label *label, SDL_Renderer *renderer, int x, int y)
{
    SDL_Rect dst;
    if (!label || !label->tex) {
        return;
    }
    dst.x = x;
    dst.y = y;
    dst.w = label->w;
    dst.h = label->h;
    SDL_RenderCopy(renderer, label->tex, NULL, &dst);
}

void hupi_label_free(hupi_label *label)
{
    if (!label) {
        return;
    }
    if (label->tex) {
        SDL_DestroyTexture(label->tex);
        label->tex = NULL;
    }
    label->text[0] = '\0';
    label->w = 0;
    label->h = 0;
}

int hupi_link_open(hupi_link *link, const char *runtime, const char *sockname)
{
    char path[256];
    int fd;
    if (link->fd >= 0) {
        return 0;
    }
    snprintf(link->runtime, sizeof link->runtime, "%s", runtime);
    snprintf(link->name, sizeof link->name, "%s", sockname);
    hupi_sock_path(path, sizeof path, runtime, sockname);
    fd = hupi_connect_unix(path);
    if (fd < 0) {
        return -1;
    }
    hupi_set_nonblock(fd);
    link->fd = fd;
    link->len = 0;
    link->subscribed = 0;
    return 0;
}

void hupi_link_close(hupi_link *link)
{
    if (link->fd >= 0) {
        close(link->fd);
    }
    link->fd = -1;
    link->len = 0;
    link->subscribed = 0;
}

int hupi_link_send(hupi_link *link, const char *line)
{
    if (link->fd < 0) {
        return -1;
    }
    if (hupi_send_line(link->fd, line) != 0) {
        hupi_link_close(link);
        return -1;
    }
    return 0;
}

int hupi_link_poll(hupi_link *link, char *line, size_t n)
{
    hupi_peer_t peer;
    int rc;
    if (link->fd < 0) {
        return -1;
    }
    memset(&peer, 0, sizeof peer);
    peer.fd = link->fd;
    if (link->len > sizeof peer.buf) {
        link->len = 0;
    }
    memcpy(peer.buf, link->buf, link->len);
    peer.len = link->len;
    rc = hupi_peer_pull(&peer, line, n);
    if (rc == 1) {
        memcpy(link->buf, peer.buf, peer.len);
        link->len = peer.len;
        return 1;
    }
    if (hupi_peer_recv(&peer) != 0) {
        hupi_link_close(link);
        return -1;
    }
    memcpy(link->buf, peer.buf, peer.len);
    link->len = peer.len;
    rc = hupi_peer_pull(&peer, line, n);
    if (rc == 1) {
        memcpy(link->buf, peer.buf, peer.len);
        link->len = peer.len;
    }
    return rc;
}

const char *hupi_runtime(int argc, char **argv)
{
    const char *runtime = getenv("HUPI_RUNTIME");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    if (runtime && runtime[0]) {
        return runtime;
    }
    return "/run/hupi";
}
