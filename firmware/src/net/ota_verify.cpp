#include "ota_verify.h"

namespace ota {

bool bin_url_is_safe(const String& url, const String& prefix) {
    // .c_str() so this compiles against both the firmware Arduino String and the
    // host test shim (whose startsWith only takes const char*).
    return prefix.length() > 0 && url.startsWith(prefix.c_str());
}

bool md5_is_valid(const String& md5) {
    if (md5.length() != 32) return false;
    for (size_t i = 0; i < 32; ++i) {
        char c = md5[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

}  // namespace ota
