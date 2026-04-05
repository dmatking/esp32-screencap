/* sh1107_sim.c — desktop backend for sh1107 using screencap.
 * On ESP32, sh1107_esp.c (not included here) handles I2C. */

#include "sh1107.h"
#include "screencap.h"

esp_err_t sh1107_init(sh1107_t *dev)
{
    dev->dirty = 0xFFFF;

    screencap_cfg_t cfg = {
        .width   = SH1107_WIDTH,
        .height  = SH1107_HEIGHT,
        .format  = SCREENCAP_MONO_PAGES,
        .scale   = 4,
        .title   = "SH1107 128x128 OLED",
        .framebuf = dev->framebuf,
    };

    /* argc/argv forwarded via globals set in main_sim.c */
    extern int  sim_argc;
    extern char **sim_argv;
    return screencap_init(&cfg, sim_argc, sim_argv);
}

esp_err_t sh1107_flush(sh1107_t *dev)
{
    if (!dev->dirty) return ESP_OK;
    screencap_flush();
    dev->dirty = 0;
    return ESP_OK;
}

void sh1107_display_off(sh1107_t *dev)
{
    (void)dev;
    screencap_destroy();
}
