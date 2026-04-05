/* Demo sim for the color_rgb565 example.
 *
 * Draws a rainbow gradient background and bouncing colored squares.
 * Press P to save screenshot_NNNN.png.
 * Run with --screenshot out.png [--frames N] for headless PNG output.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "display.h"
#include "screencap.h"

int   sim_argc;
char **sim_argv;

#define FRAME_MS  33

static void draw_gradient(display_t *d)
{
    for (int y = 0; y < DISP_HEIGHT; y++) {
        for (int x = 0; x < DISP_WIDTH; x++) {
            uint8_t r = (uint8_t)((x * 31) / (DISP_WIDTH  - 1));
            uint8_t g = (uint8_t)((y * 63) / (DISP_HEIGHT - 1));
            uint8_t b = (uint8_t)(31 - r);
            display_pixel(d, x, y, RGB565(r, g, b));
        }
    }
}

typedef struct { float x, y, vx, vy; uint16_t color; int size; } Box;

int main(int argc, char **argv)
{
    sim_argc = argc;
    sim_argv = argv;

    srand((unsigned)time(NULL));

    static display_t lcd = {0};
    if (display_init(&lcd) != ESP_OK) {
        fprintf(stderr, "Display init failed\n");
        return 1;
    }

    /* Three bouncing boxes */
    Box boxes[] = {
        { 30,  30,  1.5f,  1.1f, COLOR_RED,   24 },
        { 80,  80, -1.2f,  1.8f, COLOR_CYAN,  20 },
        { 150, 100,  1.0f, -1.4f, COLOR_YELLOW, 28 },
    };
    int n_boxes = (int)(sizeof(boxes) / sizeof(boxes[0]));

    while (screencap_poll()) {
        draw_gradient(&lcd);

        for (int i = 0; i < n_boxes; i++) {
            Box *b = &boxes[i];
            display_filled_rect(&lcd, (int)b->x, (int)b->y, b->size, b->size, b->color);
            b->x += b->vx;
            b->y += b->vy;
            if (b->x < 0 || b->x + b->size > DISP_WIDTH)  b->vx = -b->vx;
            if (b->y < 0 || b->y + b->size > DISP_HEIGHT) b->vy = -b->vy;
        }

        display_flush(&lcd);
        SDL_Delay(FRAME_MS);
    }

    screencap_destroy();
    return 0;
}
