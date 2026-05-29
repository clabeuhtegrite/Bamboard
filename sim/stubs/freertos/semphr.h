// No-op semaphore primitives for the single-threaded desktop sim.

#pragma once

#include "FreeRTOS.h"

inline SemaphoreHandle_t xSemaphoreCreateMutex()                       { return reinterpret_cast<SemaphoreHandle_t>(1); }
inline int               xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline int               xSemaphoreGive(SemaphoreHandle_t)             { return 1; }
