#include "semver.h"

namespace ota {

// Parse up to three numeric components out of "v1.2.3-rc1" -> {1,2,3}. Anything
// after a '-' or '+' (pre-release / build metadata) is ignored, which keeps the
// comparison total even for tags CI couldn't reduce to a clean triple. Each
// component is clamped so a malformed / hostile version string can't overflow.
static void parse_semver(const char* s, long out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!s) return;
    if (*s == 'v' || *s == 'V') ++s;

    int idx = 0;
    while (*s && idx < 3) {
        if (*s >= '0' && *s <= '9') {
            long v = 0;
            while (*s >= '0' && *s <= '9') {
                if (v < 1000000L) v = v * 10 + (*s - '0');   // clamp; no overflow
                ++s;
            }
            out[idx++] = v;
        } else if (*s == '.') {
            ++s;
        } else {
            break;  // '-', '+', or junk — stop here
        }
    }
}

int semver_cmp(const char* a, const char* b) {
    long va[3], vb[3];
    parse_semver(a, va);
    parse_semver(b, vb);
    for (int i = 0; i < 3; ++i) {
        if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
    }
    return 0;
}

}  // namespace ota
