# Bamboard — desktop LVGL simulator

A native SDL2 window that runs the **same** `firmware/src/ui/{screens,ui}.cpp`
sources you'd flash onto the Guition JC4827W543 board. Lets you preview
the UI at the real 480 × 272 resolution and click around with the mouse
(mouse = touch) before any hardware is on hand.

What it validates:

- LVGL screen layout and rendering at the target resolution.
- Navigation, modals, overlays (HMS flash, OTA progress, actions menu).
- Behaviour against canned Bambuddy snapshots (one printing X1C, one
  idle A1 mini, one paused P1S with an HMS error, one finished X1E
  waiting for plate clear).

What it doesn't validate:

- The display pin map in `firmware/src/hw/display.cpp` (`LovyanGFX` is
  not in the link).
- The GT911 touch driver and its I²C wiring.
- The real ESP32-S3 RGB timing parameters.
- Network paths (REST / WebSocket / OTA) — all stubbed.

## SDL2

CMake tries the system / package-manager install first, and falls back
to building SDL2 from source via `FetchContent` when nothing is found.
So the sim **just works on a fresh Windows box** — the first
configure spends 2-3 min compiling SDL2, after that it's incremental.

If you'd rather provide SDL2 yourself (faster initial configure):

| OS      | Command                                          |
|---------|--------------------------------------------------|
| Windows | `vcpkg install sdl2` then `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake` |
| macOS   | `brew install sdl2`                              |
| Linux   | `sudo apt install libsdl2-dev`                   |

## Build & run

```bash
# From the repo root
cmake -S sim -B sim/build
cmake --build sim/build --config Release

# Run (Windows: bamboard_sim.exe ; macOS / Linux: ./bamboard_sim)
sim/build/bamboard_sim
```

You should see a small 480 × 272 window with the Live dashboard. Move
your mouse around and left-click to simulate touch. Watch `stderr` for
the canned action confirmations:

```
[sim] backlight -> 255
[sim] refresh_status(1)
[sim] set_print_speed(1, mode=3)
[sim] clear_plate(4)
```

## What's in here

```
sim/
├── CMakeLists.txt              # SDL2 + LVGL + ArduinoJson fetch, target wiring
├── main.cpp                    # SDL2 init + LVGL bootstrap + event loop
├── README.md                   # you are here
└── stubs/
    ├── Arduino.h               # String, millis(), log_*, fake WiFi
    ├── ArduinoStubs.cpp        # backing impl + the WiFi singleton
    ├── WiFi.h                  # forwards to Arduino.h
    ├── WebSocketsClient.h      # minimal WStype_t + no-op WebSocketsClient
    ├── freertos/FreeRTOS.h     # SemaphoreHandle_t stub
    ├── freertos/semphr.h       # no-op xSemaphore* helpers
    ├── display_stub.cpp        # hw::g_display (no-op backlight tracking)
    └── bambuddy_stubs.cpp      # canned printer / AMS / stats data
```

The firmware's actual `hw/`, `net/` and `main.cpp` are deliberately
**not** in the sim's target list — those drag in LovyanGFX, WiFiManager,
ArduinoOTA, HTTPClient, FreeRTOS tasks, etc. that have no business on a
desktop build.

## Hacking on the UI

Edits to `firmware/src/ui/screens.cpp` or `firmware/src/ui/ui.cpp` are
picked up by both the sim and the firmware build — same TUs. Run

```bash
cmake --build sim/build
```

after every edit; CMake re-links in under a second on incremental builds.
The sim window starts dark for a frame and then redraws, so any tap
that crashes LVGL will surface immediately in the terminal output.
