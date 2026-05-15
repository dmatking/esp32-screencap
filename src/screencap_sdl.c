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
static bool             s_grid_visible;
static char             s_screenshot_path[512];
static int              s_screenshot_counter;

/* headless: run this many poll() calls before auto-saving and returning false */
static int s_headless_frames_total;
static int s_headless_frames_done;

/* Layout editor */
static screencap_elem_t      *s_elems;
static int                    s_elem_count;
static screencap_save_fn_t    s_save_fn;
static screencap_reload_fn_t  s_reload_fn;
static void                  *s_editor_ctx;
static bool                   s_editor_active;
static int                    s_selected = -1;     /* index into s_elems, -1 = none */
static bool                   s_dragging;
static int                    s_drag_offx, s_drag_offy;  /* mouse offset from elem origin */

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

/* Grid overlay — drawn on the renderer after texture copy, not into the framebuf.
 * Screenshots bypass the renderer entirely so they are always grid-free. */
#define GRID_MINOR 10
#define GRID_MAJOR 50

static void draw_grid(void)
{
    int w = s_cfg.width, h = s_cfg.height;
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
    for (int x = 0; x < w; x += GRID_MINOR) {
        SDL_SetRenderDrawColor(s_renderer, 255, 255, 255,
                               (x % GRID_MAJOR == 0) ? 60 : 25);
        SDL_RenderDrawLine(s_renderer, x, 0, x, h - 1);
    }
    for (int y = 0; y < h; y += GRID_MINOR) {
        SDL_SetRenderDrawColor(s_renderer, 255, 255, 255,
                               (y % GRID_MAJOR == 0) ? 60 : 25);
        SDL_RenderDrawLine(s_renderer, 0, y, w - 1, y);
    }
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_NONE);
}

/* Internal: called by screencap_png.c */
void screencap_fill_rgba_for_png(uint8_t *rgba) { fill_rgba(rgba); }
int  screencap_get_width(void)                  { return s_cfg.width; }
int  screencap_get_height(void)                 { return s_cfg.height; }
bool screencap_is_grid_visible(void)            { return s_grid_visible; }

/* Draw bounding boxes for all registered editor elements. Called from
 * flush() after the texture copy and grid overlay. */
static void draw_editor_overlay(void)
{
    if (!s_editor_active || !s_elems) return;
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < s_elem_count; i++) {
        SDL_Rect rect = { s_elems[i].x, s_elems[i].y, s_elems[i].w, s_elems[i].h };
        if (i == s_selected) {
            /* Selected: bright yellow, slightly inset double border */
            SDL_SetRenderDrawColor(s_renderer, 255, 220, 0, 230);
            SDL_RenderDrawRect(s_renderer, &rect);
            SDL_Rect inset = { rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2 };
            SDL_SetRenderDrawColor(s_renderer, 255, 220, 0, 130);
            SDL_RenderDrawRect(s_renderer, &inset);
        } else {
            /* Other elements: faint white border */
            SDL_SetRenderDrawColor(s_renderer, 255, 255, 255, 90);
            SDL_RenderDrawRect(s_renderer, &rect);
        }
    }
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_NONE);
}

/* Hit-test: return index of last element whose rect contains (x, y),
 * or -1. Iterating in reverse order so visually-on-top elements win
 * if rects overlap. */
static int hit_test(int x, int y)
{
    for (int i = s_elem_count - 1; i >= 0; i--) {
        const screencap_elem_t *e = &s_elems[i];
        if (x >= e->x && x < e->x + e->w &&
            y >= e->y && y < e->y + e->h)
            return i;
    }
    return -1;
}

void screencap_editor_register(screencap_elem_t      *elems,
                               int                    count,
                               screencap_save_fn_t    save_fn,
                               screencap_reload_fn_t  reload_fn,
                               void                  *ctx)
{
    s_elems       = elems;
    s_elem_count  = count;
    s_save_fn     = save_fn;
    s_reload_fn   = reload_fn;
    s_editor_ctx  = ctx;
    s_selected    = -1;
    s_dragging    = false;
}

bool screencap_editor_active(void)
{
    return s_editor_active;
}

void screencap_draw_grid_rgba(uint8_t *rgba, int w, int h)
{
    for (int x = 0; x < w; x += GRID_MINOR) {
        uint8_t a = (x % GRID_MAJOR == 0) ? 60 : 25;
        for (int y = 0; y < h; y++) {
            uint8_t *p = rgba + (y * w + x) * 4;
            p[0] = (uint8_t)(p[0] + (255 - p[0]) * a / 255);
            p[1] = (uint8_t)(p[1] + (255 - p[1]) * a / 255);
            p[2] = (uint8_t)(p[2] + (255 - p[2]) * a / 255);
        }
    }
    for (int y = 0; y < h; y += GRID_MINOR) {
        uint8_t a = (y % GRID_MAJOR == 0) ? 60 : 25;
        for (int x = 0; x < w; x++) {
            uint8_t *p = rgba + (y * w + x) * 4;
            p[0] = (uint8_t)(p[0] + (255 - p[0]) * a / 255);
            p[1] = (uint8_t)(p[1] + (255 - p[1]) * a / 255);
            p[2] = (uint8_t)(p[2] + (255 - p[2]) * a / 255);
        }
    }
}

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
        } else if (strcmp(argv[i], "--grid") == 0) {
            s_grid_visible = true;
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
    if (s_grid_visible) draw_grid();
    draw_editor_overlay();
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
            SDL_Keymod mod = SDL_GetModState();
            int nudge = (mod & KMOD_SHIFT) ? 10 : 1;
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
            case SDLK_g:
                s_grid_visible = !s_grid_visible;
                break;
            case SDLK_e:
                if (s_elems) {
                    s_editor_active = !s_editor_active;
                    if (!s_editor_active) { s_selected = -1; s_dragging = false; }
                    printf("editor: %s\n", s_editor_active ? "ON" : "OFF");
                    fflush(stdout);
                }
                break;
            case SDLK_s:
                if (s_editor_active && s_save_fn) {
                    s_save_fn(s_elems, s_elem_count, s_editor_ctx);
                }
                break;
            case SDLK_r:
                if (s_editor_active && s_reload_fn) {
                    s_reload_fn(s_editor_ctx);
                    s_selected = -1;
                }
                break;
            case SDLK_LEFT:  case SDLK_RIGHT:
            case SDLK_UP:    case SDLK_DOWN:
                if (s_editor_active && s_selected >= 0) {
                    screencap_elem_t *el = &s_elems[s_selected];
                    if (e.key.keysym.sym == SDLK_LEFT)  el->x -= nudge;
                    if (e.key.keysym.sym == SDLK_RIGHT) el->x += nudge;
                    if (e.key.keysym.sym == SDLK_UP)    el->y -= nudge;
                    if (e.key.keysym.sym == SDLK_DOWN)  el->y += nudge;
                    printf("%s: x=%d y=%d\n", el->id ? el->id : "(?)", el->x, el->y);
                    fflush(stdout);
                }
                break;
            default:
                break;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int lx = e.button.x / s_cfg.scale;
            int ly = e.button.y / s_cfg.scale;
            if (s_editor_active && s_elems) {
                int hit = hit_test(lx, ly);
                s_selected = hit;
                if (hit >= 0) {
                    s_dragging  = true;
                    s_drag_offx = lx - s_elems[hit].x;
                    s_drag_offy = ly - s_elems[hit].y;
                    printf("%s: selected at x=%d y=%d\n",
                           s_elems[hit].id ? s_elems[hit].id : "(?)",
                           s_elems[hit].x, s_elems[hit].y);
                    fflush(stdout);
                }
            } else {
                printf("click: x=%d, y=%d\n", lx, ly);
                fflush(stdout);
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            if (s_dragging && s_selected >= 0) {
                screencap_elem_t *el = &s_elems[s_selected];
                printf("%s: x=%d y=%d\n", el->id ? el->id : "(?)", el->x, el->y);
                fflush(stdout);
            }
            s_dragging = false;
        }
        if (e.type == SDL_MOUSEMOTION && s_dragging && s_selected >= 0) {
            int lx = e.motion.x / s_cfg.scale;
            int ly = e.motion.y / s_cfg.scale;
            s_elems[s_selected].x = lx - s_drag_offx;
            s_elems[s_selected].y = ly - s_drag_offy;
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
