#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screencap.h"

/* -----------------------------------------------------------------------
 * Internal state
 * ----------------------------------------------------------------------- */
static screencap_cfg_t  s_cfg;
static SDL_Window      *s_window;
static SDL_Renderer    *s_renderer;
static SDL_Texture     *s_texture;
static bool             s_headless;
static char             s_screenshot_path[512];
static int              s_screenshot_counter;

/* headless: run this many poll() calls before auto-saving and returning false */
static int s_headless_frames_total;
static int s_headless_frames_done;

/* -----------------------------------------------------------------------
 * fill_rgba: convert project framebuf → packed RGBA (width*height*4 bytes)
 * Called by both screencap_flush and screencap_save_png.
 * ----------------------------------------------------------------------- */
static void fill_rgba(uint8_t *rgba)
{
    int w = s_cfg.width;
    int h = s_cfg.height;

    switch (s_cfg.format) {

    case SCREENCAP_MONO_PAGES: {
        /* framebuf is uint8_t[h/8][w] */
        const uint8_t (*pages)[w] = (const uint8_t (*)[w])s_cfg.framebuf;
        int n_pages = h / 8;
        for (int page = 0; page < n_pages; page++) {
            for (int x = 0; x < w; x++) {
                uint8_t byte = pages[page][x];
                for (int bit = 0; bit < 8; bit++) {
                    int y = page * 8 + bit;
                    uint8_t v = ((byte >> bit) & 1) ? 255 : 0;
                    uint8_t *p = rgba + (y * w + x) * 4;
                    p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
                }
            }
        }
        break;
    }

    case SCREENCAP_RGB565: {
        /* framebuf is uint16_t[h*w] row-major */
        const uint16_t *src = (const uint16_t *)s_cfg.framebuf;
        for (int i = 0; i < w * h; i++) {
            uint16_t px = src[i];
            uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
            uint8_t g = (uint8_t)(((px >>  5) & 0x3F) * 255 / 63);
            uint8_t b = (uint8_t)(( px        & 0x1F) * 255 / 31);
            uint8_t *p = rgba + i * 4;
            p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
        }
        break;
    }

    case SCREENCAP_RGB888: {
        /* framebuf is uint8_t[h*w*3] row-major */
        const uint8_t *src = (const uint8_t *)s_cfg.framebuf;
        for (int i = 0; i < w * h; i++) {
            uint8_t *p = rgba + i * 4;
            p[0] = src[i * 3 + 0];
            p[1] = src[i * 3 + 1];
            p[2] = src[i * 3 + 2];
            p[3] = 255;
        }
        break;
    }
    }
}

/* Internal: called by screencap_png.c */
void screencap_fill_rgba_for_png(uint8_t *rgba) { fill_rgba(rgba); }
int  screencap_get_width(void)                  { return s_cfg.width; }
int  screencap_get_height(void)                 { return s_cfg.height; }

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
int screencap_init(screencap_cfg_t *cfg, int argc, char **argv)
{
    s_cfg = *cfg;
    s_headless = false;
    s_screenshot_path[0] = '\0';
    s_screenshot_counter = 0;
    s_headless_frames_total = 1;
    s_headless_frames_done = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            snprintf(s_screenshot_path, sizeof(s_screenshot_path), "%s", argv[i + 1]);
            s_headless = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            s_headless_frames_total = atoi(argv[i + 1]);
            if (s_headless_frames_total < 1) s_headless_frames_total = 1;
        }
    }

    if (s_headless) return 0;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    s_window = SDL_CreateWindow(
        cfg->title ? cfg->title : "screencap",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg->width * cfg->scale, cfg->height * cfg->scale,
        SDL_WINDOW_SHOWN
    );
    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);
    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return -1;
    }

    /* Let SDL handle integer scaling — we work in display-pixel coordinates */
    SDL_RenderSetLogicalSize(s_renderer, cfg->width, cfg->height);
    SDL_RenderSetIntegerScale(s_renderer, SDL_TRUE);

    s_texture = SDL_CreateTexture(s_renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        cfg->width, cfg->height);
    if (!s_texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}

void screencap_flush(void)
{
    if (s_headless) return;   /* PNG reads framebuf directly at save time */

    int w = s_cfg.width, h = s_cfg.height;
    uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
    if (!rgba) return;

    fill_rgba(rgba);

    SDL_UpdateTexture(s_texture, NULL, rgba, w * 4);
    free(rgba);

    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

bool screencap_poll(void)
{
    if (s_headless) {
        /* Check first so the loop body runs exactly frames_total times
         * before saving. --frames 1 = one draw, then save. */
        if (s_headless_frames_done >= s_headless_frames_total) {
            if (s_screenshot_path[0]) {
                if (screencap_save_png(s_screenshot_path) == 0)
                    printf("Saved %s\n", s_screenshot_path);
                s_screenshot_path[0] = '\0';  /* prevent double-save */
            }
            return false;
        }
        s_headless_frames_done++;
        return true;
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                return false;
            case SDLK_p: {
                char path[64];
                snprintf(path, sizeof(path), "screenshot_%04d.png", ++s_screenshot_counter);
                if (screencap_save_png(path) == 0)
                    printf("Saved %s\n", path);
                break;
            }
            default:
                break;
            }
        }
    }
    return true;
}

void screencap_destroy(void)
{
    if (s_texture)  { SDL_DestroyTexture(s_texture);   s_texture  = NULL; }
    if (s_renderer) { SDL_DestroyRenderer(s_renderer); s_renderer = NULL; }
    if (s_window)   { SDL_DestroyWindow(s_window);     s_window   = NULL; }
    SDL_Quit();
}

bool screencap_is_headless(void)
{
    return s_headless;
}
