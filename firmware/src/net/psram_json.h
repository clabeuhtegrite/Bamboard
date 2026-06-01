// Shared ArduinoJson allocator that parks JSON documents in PSRAM.
//
// ArduinoJson's default allocator uses malloc(), i.e. internal DRAM — the
// scarce ~300 KB pool the LVGL object tree and the network stack also live in.
// Bambuddy's /status and /printers payloads (8 printers × 4 AMS × 4 trays) can
// momentarily want tens of KB; routing them to the 8 MB PSRAM keeps DRAM free
// and avoids fragmenting it on every poll. Falls back to DRAM if PSRAM is
// somehow unavailable so parsing never hard-fails on a non-PSRAM board.

#pragma once

#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace bambuddy {

struct PsramAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override {
        void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
        return p ? p : heap_caps_malloc(n, MALLOC_CAP_DEFAULT);
    }
    void deallocate(void* p) override { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override {
        void* q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM);
        return q ? q : heap_caps_realloc(p, n, MALLOC_CAP_DEFAULT);
    }
};

// One shared, stateless instance (thread-safe init under C++11).
inline PsramAllocator& psram_json_allocator() {
    static PsramAllocator a;
    return a;
}

}  // namespace bambuddy
