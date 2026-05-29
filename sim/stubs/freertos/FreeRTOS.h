// Minimal FreeRTOS shim for the desktop sim. firmware/src/net/bambuddy_client.h
// keeps a mutex (`SemaphoreHandle_t mtx_`) and references portMAX_DELAY —
// the sim is single-threaded so the actual locking is a no-op, but the
// types need to exist for the header to parse on a PC build.

#pragma once

#include <cstdint>

using SemaphoreHandle_t = void*;
using TickType_t        = uint32_t;

#define portMAX_DELAY (0xFFFFFFFFu)
