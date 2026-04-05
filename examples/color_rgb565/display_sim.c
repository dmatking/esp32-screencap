/* display_sim.c — desktop backend using screencap.
 * On ESP32, display_esp.c (not included) drives the SPI LCD. */

#include "display.h"
#include "screencap.h"

esp_err_t display_init(display_t *dev)
{
    screencap_cfg_t cfg = {
        .width    = DISP_WIDTH,
        .height   = DISP_HEIGHT,
        .format   = SCREENCAP_RGB565,
        .scale    = 2,
        .title    = "RGB565 240x240 LCD",
        .framebuf = dev->framebuf,
    };

    extern int   sim_argc;
    extern char **sim_argv;
    return screencap_init(&cfg, sim_argc, sim_argv);
}

void display_flush(display_t *dev)
{
    (void)dev;
    screencap_flush();
}
