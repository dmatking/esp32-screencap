#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
#  include "esp_err.h"
#  include "driver/spi_master.h"
#else
#  include "esp_err.h"
#  include "driver/spi_master.h"   /* stub */
#endif

/* Display dimensions — adjust to match your hardware */
#define DISP_WIDTH  240
#define DISP_HEIGHT 240

/* RGB565 convenience macros */
#define RGB565(r, g, b) \
    (uint16_t)(((uint16_t)((r) & 0x1F) << 11) | \
               ((uint16_t)((g) & 0x3F) <<  5) | \
               ((uint16_t)((b) & 0x1F)))

/* Common named colors */
#define COLOR_BLACK   RGB565( 0,  0,  0)
#define COLOR_WHITE   RGB565(31, 63, 31)
#define COLOR_RED     RGB565(31,  0,  0)
#define COLOR_GREEN   RGB565( 0, 63,  0)
#define COLOR_BLUE    RGB565( 0,  0, 31)
#define COLOR_YELLOW  RGB565(31, 63,  0)
#define COLOR_CYAN    RGB565( 0, 63, 31)
#define COLOR_MAGENTA RGB565(31,  0, 31)

typedef struct {
    uint16_t framebuf[DISP_HEIGHT][DISP_WIDTH];
} display_t;

#ifdef ESP_PLATFORM
esp_err_t display_init(display_t *dev, spi_host_device_t host,
                        int pin_mosi, int pin_clk, int pin_cs,
                        int pin_dc, int pin_rst);
#else
esp_err_t display_init(display_t *dev);
#endif

void      display_flush(display_t *dev);
void      display_fill(display_t *dev, uint16_t color);
void      display_pixel(display_t *dev, int x, int y, uint16_t color);
void      display_rect(display_t *dev, int x, int y, int w, int h, uint16_t color);
void      display_filled_rect(display_t *dev, int x, int y, int w, int h, uint16_t color);
