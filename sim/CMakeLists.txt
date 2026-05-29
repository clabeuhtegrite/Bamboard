cmake_minimum_required(VERSION 3.16)
project(bamboard_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

find_package(SDL2 REQUIRED)
if (NOT SDL2_FOUND)
    message(FATAL_ERROR
        "SDL2 not found. Install it first:\n"
        "  Windows: vcpkg install sdl2 && pass -DCMAKE_TOOLCHAIN_FILE=...\n"
        "  macOS  : brew install sdl2\n"
        "  Linux  : sudo apt install libsdl2-dev")
endif()

include(FetchContent)

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
    ${SDL2_INCLUDE_DIRS}
)

# The Arduino / WiFi / FreeRTOS / WebSocketsClient shims live in stubs/
# and must be visible to the firmware sources *before* the real headers
# (which don't exist on a PC build).
target_compile_definitions(bamboard_sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE=1
)

# SDL2 ships either a SDL2::SDL2 target (modern installs / vcpkg) or
# only raw SDL2_LIBRARIES (older find modules). Handle both.
if (TARGET SDL2::SDL2)
    target_link_libraries(bamboard_sim PRIVATE SDL2::SDL2)
else()
    target_link_libraries(bamboard_sim PRIVATE ${SDL2_LIBRARIES})
endif()

target_link_libraries(bamboard_sim PRIVATE lvgl ArduinoJson)

# Quieter warnings on the firmware sources — they target an MCU, not a
# desktop, so the odd unused-parameter / sign-comparison nit is fine.
if (NOT MSVC)
    target_compile_options(bamboard_sim PRIVATE
        -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-declarations)
endif()
