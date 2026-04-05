/* PNG save implementation using stb_image_write (header-only, vendored). */

#include <stdio.h>
#include <stdlib.h>
#include "screencap.h"

/* Bridge to screencap_sdl.c — avoids duplicating format conversion */
void screencap_fill_rgba_for_png(uint8_t *rgba);
int  screencap_get_width(void);
int  screencap_get_height(void);

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

int screencap_save_png(const char *path)
{
    int w = screencap_get_width();
    int h = screencap_get_height();

    if (w == 0 || h == 0) {
        fprintf(stderr, "screencap_save_png: call screencap_init first\n");
        return -1;
    }

    uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
    if (!rgba) return -1;

    screencap_fill_rgba_for_png(rgba);

    int ok = stbi_write_png(path, w, h, 4, rgba, w * 4);
    free(rgba);

    if (!ok) {
        fprintf(stderr, "screencap_save_png: failed to write '%s'\n", path);
        return -1;
    }
    return 0;
}
