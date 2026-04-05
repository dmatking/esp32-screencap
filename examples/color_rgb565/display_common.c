/* Portable framebuffer operations — compiled for both ESP32 and desktop sim. */

#include "display.h"
#include <string.h>

void display_fill(display_t *dev, uint16_t color)
{
    for (int i = 0; i < DISP_HEIGHT * DISP_WIDTH; i++)
        ((uint16_t *)dev->framebuf)[i] = color;
}

void display_pixel(display_t *dev, int x, int y, uint16_t color)
{
    if (x < 0 || x >= DISP_WIDTH || y < 0 || y >= DISP_HEIGHT) return;
    dev->framebuf[y][x] = color;
}

void display_rect(display_t *dev, int x, int y, int w, int h, uint16_t color)
{
    for (int i = x; i < x + w; i++) {
        display_pixel(dev, i, y,         color);
        display_pixel(dev, i, y + h - 1, color);
    }
    for (int i = y; i < y + h; i++) {
        display_pixel(dev, x,         i, color);
        display_pixel(dev, x + w - 1, i, color);
    }
}

void display_filled_rect(display_t *dev, int x, int y, int w, int h, uint16_t color)
{
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            display_pixel(dev, col, row, color);
}
