# esp32-screencap

Native Linux/Windows desktop simulator for ESP32 display projects.

Compile your existing ESP32 display code unchanged, see what the screen looks like in a live SDL2 window, and save PNG screenshots — useful for README images, CI, and development without hardware.

![mono OLED demo](examples/mono_sh1107/build/mono_test.png) ![color LCD demo](examples/color_rgb565/build/color_test.png)

## How it works

Your project's display driver has two implementations:

| File | Used when |
|---|---|
| `display_esp.c` | Building for ESP32 with ESP-IDF |
| `display_sim.c` | Building for desktop (calls screencap) |

The sim build uses a native CMake project that:
1. Includes `esp_stubs/` headers to shadow ESP-IDF includes
2. Links SDL2 for the live preview window
3. Uses `stb_image_write` (vendored, header-only) for PNG output

The project's framebuffer is pointed to directly — no copy, no protocol change.

## Supported display formats

| `screencap_format_t` | Description | Common hardware |
|---|---|---|
| `SCREENCAP_MONO_PAGES` | 1-bpp, 8-rows-per-page | SH1107, SSD1306, SSD1309 |
| `SCREENCAP_RGB565` | 16-bpp packed, row-major | ST7789, ILI9341, ST7703 |
| `SCREENCAP_RGB888` | 24-bpp, row-major | generic RGB framebuffer |

## Dependencies

```
sudo apt install libsdl2-dev
```

No other runtime dependencies. PNG output uses vendored `stb_image_write.h`.

## Quick start

Clone as a submodule in your project:

```bash
git submodule add https://github.com/dmatking/esp32-screencap.git sim/screencap
```

Create `sim/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(myproject_sim C)

include(screencap/cmake/screencap.cmake)

screencap_add_sim(myproject_sim
    SOURCES
        main_sim.c          # your sim entry point
        display_sim.c       # calls screencap_init / screencap_flush
        ../main/game.c      # your existing app code (unchanged)
        ../main/render.c
    INCLUDES
        ../main             # your project's headers
)
```

Build:

```bash
mkdir sim/build && cd sim/build
cmake ..
make
./myproject_sim
```

## Keyboard shortcuts (SDL window)

| Key | Action |
|---|---|
| `P` | Save `screenshot_NNNN.png` (auto-increments) |
| `G` | Toggle a 10/50 px minor/major grid overlay (alignment aid) |
| `E` | Toggle layout edit mode (only if the project registered an editor — see below) |
| Click | Print board-space coords to stdout (or select an editor element if edit mode is on) |
| `Esc` / close | Quit |

## Layout editor (optional)

Projects with hand-positioned UI elements can register an editor session and drag elements around the live window. screencap handles all the UI (selection, drag math, arrow-key nudge, save callback); the project owns the underlying coordinates.

```c
static screencap_elem_t elems[] = {
    { .id = "title",  .x = 4,  .y = 4,  .w = 256, .h = 16 },
    { .id = "score",  .x = 4,  .y = 28, .w = 64,  .h = 16 },
    /* … */
};

static void save_layout(const screencap_elem_t *e, int n, void *ctx)
{
    /* Write your positions back to source / a generated header. */
}

screencap_editor_register(elems, sizeof(elems)/sizeof(elems[0]),
                          save_layout, NULL /* reload */, NULL /* ctx */);
```

Once registered, the editor responds to:

| Key | Action |
|---|---|
| `E` | Toggle edit mode (highlights all rects, enables selection) |
| Click | Select the topmost element under the cursor |
| Drag | Move the selected element |
| Arrows | Nudge selected element by 1 px (Shift = 10 px) |
| `S` | Invoke the save callback |
| `R` | Invoke the reload callback |

Element rects are in **board-space pixels** (same coords as the framebuffer); screencap handles the window-scale conversion. screencap writes the new x/y back into the array the project passes — call `screencap_editor_active()` if you need to know whether edit mode is on.

## Headless / CI mode

```bash
./myproject_sim --screenshot output.png
./myproject_sim --screenshot output.png --frames 60   # run 60 ticks first
./myproject_sim --screenshot output.png --grid        # bake grid overlay into the PNG
```

Returns exit code 0 on success. Can be run in a CI job with a virtual framebuffer (`Xvfb`) or headless SDL (`SDL_VIDEODRIVER=offscreen`).

## Writing a display_sim.c

```c
#include "display.h"
#include "screencap.h"

/* argc/argv forwarded from main_sim.c */
extern int   sim_argc;
extern char **sim_argv;

esp_err_t display_init(display_t *dev)
{
    screencap_cfg_t cfg = {
        .width    = DISP_WIDTH,
        .height   = DISP_HEIGHT,
        .format   = SCREENCAP_RGB565,
        .scale    = 2,             /* window zoom factor */
        .title    = "My Project",
        .framebuf = dev->framebuf, /* point at your existing buffer */
    };
    return screencap_init(&cfg, sim_argc, sim_argv);
}

void display_flush(display_t *dev)
{
    (void)dev;
    screencap_flush();
}
```

Then in your sim main loop:

```c
while (screencap_poll()) {
    app_tick();          /* your existing logic */
    display_flush(&lcd);
    SDL_Delay(33);       /* ~30 fps */
}
screencap_destroy();
```

## ESP-IDF stubs provided

| Header | What it stubs |
|---|---|
| `esp_err.h` | `esp_err_t`, `ESP_OK`, `ESP_FAIL` |
| `esp_log.h` | `ESP_LOGI/W/E/D/V` → `printf` |
| `esp_random.h` | `esp_random()` → `rand()` |
| `esp_timer.h` | `esp_timer_get_time()` → `clock_gettime` |
| `driver/i2c_master.h` | handle typedefs |
| `driver/spi_master.h` | handle typedefs, `SPI2_HOST`/`SPI3_HOST` |
| `driver/gpio.h` | `gpio_num_t` |
| `freertos/FreeRTOS.h` | `TickType_t`, `pdTRUE`, `pdMS_TO_TICKS` |
| `freertos/task.h` | `vTaskDelay()` → `usleep` |

For anything not listed, add a stub header to your project's `sim/` directory and include it before the `screencap/` path — it will shadow the missing header.

## Examples

| Example | Display | What it shows |
|---|---|---|
| [`examples/mono_sh1107/`](examples/mono_sh1107/) | SH1107 128×128 OLED | Bouncing ball, frame counter |
| [`examples/color_rgb565/`](examples/color_rgb565/) | Generic 240×240 RGB565 | Rainbow gradient, bouncing colored squares |

Build an example:

```bash
cd examples/mono_sh1107
mkdir build && cd build
cmake ..
make
./mono_sh1107_sim
```
