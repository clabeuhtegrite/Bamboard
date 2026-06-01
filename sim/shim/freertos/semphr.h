// Host shim for <freertos/semphr.h> — mutex over std::mutex.
#pragma once
#include <cstdint>
#include <mutex>

typedef std::mutex* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::mutex(); }
inline int xSemaphoreTake(SemaphoreHandle_t m, uint32_t) { if (m) m->lock(); return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t m) { if (m) m->unlock(); return 1; }
