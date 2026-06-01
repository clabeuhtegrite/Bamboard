// Host shim for <esp_heap_caps.h> — plain malloc/realloc/free.
#pragma once
#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_EXEC      0
#define MALLOC_CAP_DMA       0
#define MALLOC_CAP_SPIRAM    0
#define MALLOC_CAP_INTERNAL  0
#define MALLOC_CAP_DEFAULT   0

inline void*  heap_caps_malloc(size_t n, uint32_t) { return malloc(n); }
inline void*  heap_caps_realloc(void* p, size_t n, uint32_t) { return realloc(p, n); }
inline void   heap_caps_free(void* p) { free(p); }
inline size_t heap_caps_get_free_size(uint32_t) { return 4u * 1024u * 1024u; }
