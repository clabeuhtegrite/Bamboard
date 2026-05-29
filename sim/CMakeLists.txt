cmake_minimum_required(VERSION 3.16)
project(bamboard_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# Match the firmware's pinned versions so the same screens.cpp behaves
# identically on the desktop sim and on the board.
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v8.3.11
)
FetchContent_Declare(
    ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v7.1.0
)

FetchContent_MakeAvailable(lvgl ArduinoJson)

# LVGL needs to be told where to find lv_conf.h. We point it at the same
# header the firmware uses so colour depth / fonts / tick settings are
# byte-identical between sim and board. `stubs/` is also visible so
# LVGL's lv_tick.c can include "Arduino.h" (via LV_TICK_CUSTOM_INCLUDE)
# and resolve our C-callable millis() definition.
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
target_include_directories(lvgl PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/include
    ${CMAKE_CURRENT_LIST_DIR}/stubs
)

# SDL2 — try the system / vcpkg / brew install first (fast path on
# Linux + macOS), fall back to fetching the official source if nothing
# is found (the no-install path that "just works" on a fresh Windows
# box). Building SDL2 from source adds 2-3 min to the first configure
# but produces a self-contained static lib so the resulting .exe has
# no external runtime dependency.
find_package(SDL2 QUIET CONFIG)

if (NOT SDL2_FOUND)
    message(STATUS "SDL2 not found via find_package — fetching from libsdl-org/SDL.")
    set(SDL_SHARED           OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC           ON  CACHE BOOL "" FORCE)
    set(SDL_TEST             OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.7
    )
    FetchContent_MakeAvailable(SDL2)
endif()

# -----------------------------------------------------------------------------
# Sim target
# -----------------------------------------------------------------------------

set(FIRMWARE_SRC ${CMAKE_CURRENT_LIST_DIR}/../firmware/src)

add_executable(bamboard_sim
    main.cpp

    # Firmware sources compiled as-is — these are display- and
    # network-agnostic; everything below this line is from the same TUs
    # that flash onto the Guition board.
    ${FIRMWARE_SRC}/ui/screens.cpp
    ${FIRMWARE_SRC}/ui/ui.cpp

    # Sim stubs that replace the on-device hw + net layers.
    stubs/ArduinoStubs.cpp
    stubs/bambuddy_stubs.cpp
    stubs/display_stub.cpp
)

target_include_directories(bamboard_sim PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/stubs              # MUST come first so our Arduino.h
                                                  # shim shadows any system Arduino.h
    ${CMAKE_CURRENT_LIST_DIR}/../firmware/include
    ${FIRMWARE_SRC}
)

target_compile_definitions(bamboard_sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE=1
    # MSVC otherwise emits a wall of C4996 deprecation warnings about
    # the regular C runtime calls (strncpy, snprintf) the firmware uses.
    # GCC / clang have no such concept; we only want to disarm them in
    # the desktop build.
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS=1>
)

# SDL2 link wiring — handle every shape of SDL2 install we might find.
# Order matters: `SDL2::SDL2-static` exists when we built via
# FetchContent with SDL_STATIC=ON; `SDL2::SDL2` is what vcpkg /
# Homebrew / apt-side configs export; raw variables are the legacy
# fallback.
if (TARGET SDL2::SDL2-static)
    target_link_libraries(bamboard_sim PRIVATE SDL2::SDL2-static)
elseif (TARGET SDL2::SDL2)
    target_link_libraries(bamboard_sim PRIVATE SDL2::SDL2)
else()
    target_link_libraries(bamboard_sim PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(bamboard_sim PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

# SDL2main wraps WinMain on Windows and SDL_main on macOS so our plain
# `int main(int, char**)` works as the entry point.
if (TARGET SDL2::SDL2main)
    target_link_libraries(bamboard_sim PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(bamboard_sim PRIVATE lvgl ArduinoJson)

# Quieter warnings on the firmware sources — they target an MCU, not a
# desktop, so the odd unused-parameter / sign-comparison nit is fine.
if (NOT MSVC)
    target_compile_options(bamboard_sim PRIVATE
        -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-declarations)
else()
    # MSVC's default warning level on a stranger codebase is noisy; bump
    # the firmware sources back to default rather than /W4 so the
    # configure output stays readable.
    target_compile_options(bamboard_sim PRIVATE /W3 /wd4244 /wd4267 /wd4018)
endif()
