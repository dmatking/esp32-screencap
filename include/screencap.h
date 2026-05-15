#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Framebuffer pixel formats
 * ----------------------------------------------------------------------- */
typedef enum {
    /* 1-bpp, page-addressed: framebuf is uint8_t[height/8][width].
     * Each byte holds 8 vertical pixels; bit 0 = topmost row of page.
     * Used by SH1107, SSD1306, SSD1309. */
    SCREENCAP_MONO_PAGES,

    /* 16-bpp packed RGB565, row-major: framebuf is uint16_t[height][width].
     * Used by ST7789, ILI9341, ST7703. */
    SCREENCAP_RGB565,

    /* 24-bpp RGB888, row-major: framebuf is uint8_t[height][width * 3]. */
    SCREENCAP_RGB888,
} screencap_format_t;

/* -----------------------------------------------------------------------
 * Configuration (fill before calling screencap_init)
 * ----------------------------------------------------------------------- */
typedef struct {
    int                 width;      /* display width in pixels */
    int                 height;     /* display height in pixels */
    screencap_format_t  format;
    int                 scale;      /* integer zoom: 4 → 512px window for 128px display */
    const char         *title;      /* SDL window title */
    const void         *framebuf;   /* pointer to project's framebuffer; must remain valid */
} screencap_cfg_t;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */

/* Open the SDL window (or skip if --screenshot path was supplied on argv).
 * Call once before the render loop. */
int screencap_init(screencap_cfg_t *cfg, int argc, char **argv);

/* Blit the current contents of cfg->framebuf to the SDL window.
 * If headless (--screenshot mode), this is a no-op. */
void screencap_flush(void);

/* Save the current framebuffer as a PNG at `path`.
 * Derives pixel data from the same framebuf pointer given to screencap_init. */
int screencap_save_png(const char *path);

/* Pump SDL events.
 * Returns false when the user closes the window or presses Escape.
 * Pressing 'P' auto-saves screenshot_NNNN.png.
 * Pressing 'G' toggles a grid overlay (10px minor / 50px major; not in screenshots).
 * Clicking the window prints board-space coordinates to stdout (layout aid).
 * In headless mode always returns false after one call (caller should exit). */
bool screencap_poll(void);

/* Tear down SDL. Safe to call even if screencap_init was never called. */
void screencap_destroy(void);

/* True if invoked with --screenshot (headless PNG mode). */
bool screencap_is_headless(void);

/* -----------------------------------------------------------------------
 * Layout editor (optional)
 *
 * Lets the project register a set of named bounding rects that the user
 * can drag with the mouse / nudge with arrow keys while the sim window
 * is in edit mode. Screencap handles all UI (selection rect, drag math,
 * keyboard handling); the project owns the underlying coordinates and
 * any persistence.
 *
 * Element ownership: the array is owned by the project. Screencap reads
 * and *writes* x/y as the user manipulates elements. The project
 * propagates those values to wherever they're consumed (e.g. a draw
 * function reading from the same memory).
 *
 * Keys (active only when an editor has been registered):
 *   E              Toggle edit mode on/off.
 *   Click          Select the topmost element under the cursor.
 *   Mouse drag     Move the selected element.
 *   Arrows         Nudge selected element by 1 px (Shift = 10 px).
 *   S              Invoke the save callback (project writes to disk).
 *   R              Invoke the reload callback (project re-reads from disk).
 *
 * Element rects are in *board-space* pixels (same coordinate system as
 * the framebuffer). Screencap converts to/from window-space using the
 * cfg.scale factor automatically.
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *id;        /* stable identifier, e.g. "views_label" */
    int x, y, w, h;        /* bounding rect; w/h drive hit-testing only */
} screencap_elem_t;

typedef void (*screencap_save_fn_t)(const screencap_elem_t *elems,
                                    int count, void *ctx);
typedef void (*screencap_reload_fn_t)(void *ctx);

/* Register an editor session. `elems` must remain valid for as long as
 * screencap is running; screencap may write into its members.
 *
 * Either callback may be NULL — pass NULL for save_fn to make the editor
 * read-only (no S keybind), NULL for reload_fn to disable R. */
void screencap_editor_register(screencap_elem_t      *elems,
                               int                    count,
                               screencap_save_fn_t    save_fn,
                               screencap_reload_fn_t  reload_fn,
                               void                  *ctx);

/* True if edit mode is currently active. The project draw function may
 * use this to skip rendering its own background ornaments if desired. */
bool screencap_editor_active(void);

#ifdef __cplusplus
}
#endif
