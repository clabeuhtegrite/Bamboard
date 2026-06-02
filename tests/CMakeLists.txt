# Host unit tests for Bamboard's pure logic (no HW, no network, no secrets).
# Reuses the sim's Arduino String shim for the few String-based validators, so
# this stays a tiny, dependency-free build (no LVGL / ArduinoJson / curl fetch).
cmake_minimum_required(VERSION 3.16)
project(bamboard_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)

add_executable(bamboard_tests
  test_logic.cpp
  ${FW}/net/semver.cpp
  ${FW}/net/host_valid.cpp
  ${FW}/ui/i18n.cpp
  ${FW}/ui/dry_default.cpp
)
# Shim dir first so <Arduino.h> resolves to the host String; then the firmware
# tree so "net/..." / "ui/..." includes resolve.
target_include_directories(bamboard_tests BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../sim/shim
  ${FW}
)

enable_testing()
add_test(NAME unit COMMAND bamboard_tests)
