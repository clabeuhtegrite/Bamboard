# Bamboard host simulator — Claude's visual-validation harness.
#
# Compiles the REAL firmware UI (firmware/src/ui/**) + the REAL Bambuddy REST
# client against host shims (sim/shim) and LVGL built for a memory framebuffer,
# then renders each screen to a PNG. Networking (milestone 2) goes through
# libcurl with Cloudflare Access service-token headers so it can reach the
# user's real Bambuddy. There is NO fabricated printer data.
#
# This is a developer/CI tool only — it never ships to the device.

cmake_minimum_required(VERSION 3.16)
project(bamboard_sim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include(FetchContent)

# --- LVGL 8.3.11 (same minor as the firmware) ---
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG        v8.3.11)
# Point LVGL at our host lv_conf.h (absolute path; propagates to consumers via
# the lvgl target's PUBLIC compile definitions).
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)

# --- ArduinoJson 7 (same major as the firmware) ---
FetchContent_Declare(ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG        v7.1.0)
FetchContent_MakeAvailable(ArduinoJson)

# --- stb_image (single-header JPEG decoder) ---
# Header-only with no CMake project, so populate (don't add_subdirectory). It
# backs the TJpg_Decoder host shim so the sim can decode the real printer camera.
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4)  # pinned 2026-04-15
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
endif()

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)

# Real firmware sources we want to validate (UI + REST client + fonts + i18n).
file(GLOB FW_FONTS ${FW}/ui/fonts/*.c)
set(FW_SRC
  ${FW}/ui/ui.cpp
  ${FW}/ui/i18n.cpp
  ${FW}/ui/screens/dashboard.cpp
  ${FW}/ui/screens/ams.cpp
  ${FW}/ui/screens/printers.cpp
  ${FW}/ui/screens/queue.cpp
  ${FW}/ui/screens/history.cpp
  ${FW}/ui/screens/settings.cpp
  ${FW}/ui/screens/header.cpp
  ${FW}/ui/screens/tab_bar.cpp
  ${FW}/ui/screens/overlays.cpp
  ${FW}/ui/screens/theme.cpp
  ${FW}/net/bambuddy_client.cpp
  ${FW}/net/bambuddy_ws.cpp
)

add_executable(bamboard_sim
  main.cpp
  shim/shims.cpp
  ${FW_SRC}
  ${FW_FONTS}
)

# Shims must win over any system header of the same name → put them first.
target_include_directories(bamboard_sim BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shim
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${FW}
  ${stb_SOURCE_DIR}
)
target_compile_definitions(bamboard_sim PRIVATE
  BAMBOARD_SIM=1
  # Turn on ONLY ArduinoJson's Arduino-Stream adapter (so deserializeJson can
  # read HTTPClient::getStream(), our ::Stream shim). Leave its Arduino String
  # / Print / PROGMEM glue off — our shims don't implement concat()/Printable
  # and the firmware never asks ArduinoJson to produce a String anyway.
  ARDUINOJSON_ENABLE_ARDUINO_STREAM=1
  ARDUINOJSON_ENABLE_ARDUINO_STRING=0
  ARDUINOJSON_ENABLE_ARDUINO_PRINT=0
  ARDUINOJSON_ENABLE_PROGMEM=0
)
target_link_libraries(bamboard_sim PRIVATE lvgl ArduinoJson)

# libcurl for the real Bambuddy fetch (milestone 2). Optional at configure time
# so milestone-1 (empty-state render) still builds without it.
find_package(CURL)
if(CURL_FOUND)
  target_link_libraries(bamboard_sim PRIVATE CURL::libcurl)
  target_compile_definitions(bamboard_sim PRIVATE SIM_HAVE_CURL=1)
endif()
