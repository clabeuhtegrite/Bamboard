// =============================================================================
// Bamboard — desktop LVGL simulator
// =============================================================================
//
// Bootstraps SDL2 + LVGL into a 480×272 window so you can preview the same
// firmware/src/ui/{screens,ui}.cpp code that runs on the Guition board.
// Mouse drives the LVGL pointer input device; the canned Bambuddy data in
// stubs/bambuddy_stubs.cpp makes every screen look populated.
//
// Build + run:
//     cmake -S sim -B sim/build
//     cmake --build sim/build
//     sim/build/bamboard_sim     (or .exe on Windows)
//
// See sim/README.md for per-OS install steps.

#include <SDL.h>
#include <lvgl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../firmware/src/config.h"
#include "../firmware/src/ui/screens.h"
#include "../firmware/src/ui/ui.h"

// --- LVGL ↔ SDL plumbing ----------------------------------------------------

static SDL_Window*   s_window   = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static SDL_Texture*  s_texture  = nullptr;
static uint16_t*     s_fb       = nullptr;

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
static lv_indev_drv_t     s_indev_drv;

static int  s_mouse_x = 0;
static int  s_mouse_y = 0;
static bool s_mouse_pressed = false;

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    int w  = area->x2 - area->x1 + 1;
    int h  = area->y2 - area->y1 + 1;

    SDL_Rect rect{area->x1, area->y1, w, h};
    SDL_UpdateTexture(s_texture, &rect, buf, w * sizeof(uint16_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
    SDL_RenderPresent(s_renderer);

    lv_disp_flush_ready(drv);
}

static void mouse_read_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state   = s_mouse_pressed ? LV_INDEV_STATE_PRESSED
                                    : LV_INDEV_STATE_RELEASED;
}

// --- main --------------------------------------------------------------------

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    s_window = SDL_CreateWindow(
        "Bamboard — sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ::display::WIDTH, ::display::HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!s_window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);
    if (!s_renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }

    s_texture = SDL_CreateTexture(
        s_renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        ::display::WIDTH, ::display::HEIGHT);
    if (!s_texture) {
        std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return 1;
    }

    lv_init();

    // Draw buffers live in plain heap on the sim — no PSRAM trick needed.
    size_t pixels = ::display::WIDTH * ::display::HEIGHT;
    static lv_color_t* buf1 = new lv_color_t[pixels];
    static lv_color_t* buf2 = new lv_color_t[pixels];
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = ::display::WIDTH;
    s_disp_drv.ver_res  = ::display::HEIGHT;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = mouse_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    // Bring up the same UI manager the firmware uses.
    ui::g_ui.begin();

    std::fprintf(stderr,
        "Bamboard sim running.\n"
        "  Window: %dx%d, mouse = touch.\n"
        "  Close the window or press Ctrl-C to quit.\n",
        (int)::display::WIDTH, (int)::display::HEIGHT);

    uint32_t last_refresh = 0;
    bool     running      = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEMOTION:
                    s_mouse_x = ev.motion.x;
                    s_mouse_y = ev.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    s_mouse_x = ev.button.x;
                    s_mouse_y = ev.button.y;
                    if (ev.button.button == SDL_BUTTON_LEFT)
                        s_mouse_pressed = true;
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button == SDL_BUTTON_LEFT)
                        s_mouse_pressed = false;
                    break;
                default:
                    break;
            }
        }

        lv_timer_handler();

        uint32_t now = SDL_GetTicks();
        if (now - last_refresh > 250) {
            ui::g_ui.refresh();
            last_refresh = now;
        }

        SDL_Delay(5);
    }

    SDL_DestroyTexture (s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow  (s_window);
    SDL_Quit();
    return 0;
}
