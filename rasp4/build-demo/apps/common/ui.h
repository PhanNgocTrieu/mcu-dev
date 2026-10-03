#ifndef HUPI_UI_H
#define HUPI_UI_H

#include <SDL.h>

#include <stddef.h>

typedef struct hupi_font hupi_font;

typedef struct {
    SDL_Texture *tex;
    char text[192];
    SDL_Color color;
    int w;
    int h;
} hupi_label;

hupi_font *hupi_font_open(float px);
void hupi_font_close(hupi_font *font);

void hupi_label_set(hupi_label *label, SDL_Renderer *renderer, hupi_font *font, SDL_Color color,
                    const char *text);
void hupi_label_draw(hupi_label *label, SDL_Renderer *renderer, int x, int y);
void hupi_label_free(hupi_label *label);

typedef struct {
    int fd;
    char buf[4096];
    size_t len;
    char runtime[160];
    char name[64];
    int subscribed;
} hupi_link;

int hupi_link_open(hupi_link *link, const char *runtime, const char *sockname);
void hupi_link_close(hupi_link *link);
int hupi_link_send(hupi_link *link, const char *line);
/* 1 = line, 0 = nothing, -1 = dropped (caller may reopen). */
int hupi_link_poll(hupi_link *link, char *line, size_t n);

const char *hupi_runtime(int argc, char **argv);

#endif
