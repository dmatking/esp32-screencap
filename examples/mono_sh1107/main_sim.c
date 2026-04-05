/* Demo sim for the mono_sh1107 example.
 *
 * Draws a bouncing ball and a frame counter to show animation works.
 * Press P to save screenshot_NNNN.png.
 * Run with --screenshot out.png [--frames N] for headless PNG output.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "sh1107.h"
#include "screencap.h"

/* Forwarded to sh1107_sim.c so it can call screencap_init with real argc/argv */
int   sim_argc;
char **sim_argv;

#define FRAME_MS  33   /* ~30 fps */

static void draw_rect(sh1107_t *d, int x, int y, int w, int h)
{
    for (int i = x; i < x + w; i++) {
        sh1107_pixel(d, i, y,         true);
        sh1107_pixel(d, i, y + h - 1, true);
    }
    for (int i = y; i < y + h; i++) {
        sh1107_pixel(d, x,         i, true);
        sh1107_pixel(d, x + w - 1, i, true);
    }
}

static void draw_filled_circle(sh1107_t *d, int cx, int cy, int r)
{
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r)
                sh1107_pixel(d, x, y, true);
        }
    }
}

int main(int argc, char **argv)
{
    sim_argc = argc;
    sim_argv = argv;

    srand((unsigned)time(NULL));

    static sh1107_t oled = {0};
    if (sh1107_init(&oled) != ESP_OK) {
        fprintf(stderr, "Display init failed\n");
        return 1;
    }

    /* Ball state */
    float bx = 20.0f, by = 20.0f;
    float vx =  1.7f, vy =  1.3f;
    const int R = 6;
    int frame = 0;
    char buf[32];

    while (screencap_poll()) {
        sh1107_clear(&oled);

        /* Border */
        draw_rect(&oled, 0, 0, SH1107_WIDTH, SH1107_HEIGHT);

        /* Ball */
        draw_filled_circle(&oled, (int)bx, (int)by, R);

        /* Frame counter */
        snprintf(buf, sizeof(buf), "frame %d", frame);
        sh1107_puts(&oled, 4, SH1107_HEIGHT - 10, buf);

        sh1107_flush(&oled);

        /* Physics */
        bx += vx;
        by += vy;
        if (bx - R < 1 || bx + R > SH1107_WIDTH  - 2) vx = -vx;
        if (by - R < 1 || by + R > SH1107_HEIGHT - 2) vy = -vy;
        frame++;

        SDL_Delay(FRAME_MS);
    }

    screencap_destroy();
    return 0;
}
