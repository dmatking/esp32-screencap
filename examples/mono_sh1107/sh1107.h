#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
#  include "driver/i2c_master.h"
#  include "esp_err.h"
#else
#  include "esp_err.h"   /* pulled from esp_stubs/ by CMake include path */
#endif

#define SH1107_WIDTH  128
#define SH1107_HEIGHT 128
#define SH1107_PAGES  (SH1107_HEIGHT / 8)

typedef struct {
#ifdef ESP_PLATFORM
    i2c_master_dev_handle_t i2c_dev;
#endif
    uint8_t framebuf[SH1107_PAGES][SH1107_WIDTH];
    uint16_t dirty;  /* bitmask: one bit per page */
} sh1107_t;

#ifdef ESP_PLATFORM
esp_err_t sh1107_init(sh1107_t *dev, i2c_master_bus_handle_t bus, uint8_t addr);
#else
esp_err_t sh1107_init(sh1107_t *dev);
#endif

void      sh1107_clear(sh1107_t *dev);
esp_err_t sh1107_flush(sh1107_t *dev);
void      sh1107_pixel(sh1107_t *dev, int x, int y, bool on);
int       sh1107_putc(sh1107_t *dev, int x, int y, char c);
void      sh1107_puts(sh1107_t *dev, int x, int y, const char *s);
void      sh1107_display_off(sh1107_t *dev);
