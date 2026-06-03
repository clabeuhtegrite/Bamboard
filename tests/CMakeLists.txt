# Host unit tests for Bamboard's pure logic (no HW, no network, no secrets).
# Reuses the sim's Arduino shims so the firmware sources compile on the host:
# the String/Stream/FreeRTOS/heap_caps shims stand in for the Arduino + ESP
# APIs, and a tiny tests-local shim (host_net_shim.cpp) supplies the few
# non-inline net symbols the REST client links against (its network paths are
# never executed — the tests only drive the apply_*_payload JSON parsers).
cmake_minimum_required(VERSION 3.16)
project(bamboard_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ArduinoJson 7 (same major as the firmware) — the Bambuddy REST client parses
# with it, and these tests exercise that parsing directly. Header-only, so this
# stays a quick build (no LVGL / LovyanGFX / curl).
FetchContent_Declare(ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG        v7.1.0)
FetchContent_MakeAvailable(ArduinoJson)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)

add_executable(bamboard_tests
  test_logic.cpp
  host_net_shim.cpp
  ${FW}/net/semver.cpp
  ${FW}/net/host_valid.cpp
  ${FW}/net/bambuddy_client.cpp
  ${FW}/ui/i18n.cpp
  ${FW}/ui/dry_default.cpp
)
# Shim dir first so <Arduino.h> & friends resolve to the host stand-ins; then the
# firmware tree so "net/..." / "ui/..." includes resolve. ArduinoJson's include
# comes in via the linked target below (neither shim nor firmware shadows it).
target_include_directories(bamboard_tests BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../sim/shim
  ${FW}
)
# Match the simulator's ArduinoJson configuration so the REST client compiles
# against our host Stream/String shims exactly as it does there (it reads from
# an Arduino ::Stream; it never asks ArduinoJson to produce an Arduino String).
target_compile_definitions(bamboard_tests PRIVATE
  ARDUINOJSON_ENABLE_ARDUINO_STREAM=1
  ARDUINOJSON_ENABLE_ARDUINO_STRING=0
  ARDUINOJSON_ENABLE_ARDUINO_PRINT=0
  ARDUINOJSON_ENABLE_PROGMEM=0
)
target_link_libraries(bamboard_tests PRIVATE ArduinoJson)

enable_testing()
add_test(NAME unit COMMAND bamboard_tests)
