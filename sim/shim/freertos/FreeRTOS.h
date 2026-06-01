// Host shim for <freertos/FreeRTOS.h>.
#pragma once
#include <cstdint>

#define portMAX_DELAY 0xFFFFFFFFu
typedef uint32_t TickType_t;
typedef int BaseType_t;

inline uint32_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
inline void vTaskDelay(uint32_t) {}
