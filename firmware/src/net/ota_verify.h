// Pure OTA-manifest verification predicates, split out of github_ota.cpp so the
// security-critical checks can be unit-tested on the host (github_ota.cpp itself
// drags in HTTPClient / Update / WiFi and can't be linked into the test binary).
//
// Both are the MITM barriers the device relies on once TLS has authenticated
// GitHub: the binary URL must sit under our own release-download path (a
// tampered manifest can't redirect the flash elsewhere), and the integrity hash
// the flash is bound to must be a real 32-hex-char MD5 (no md5 ⇒ refuse).

#pragma once

#include <Arduino.h>

namespace ota {

// True iff `url` begins with `prefix` (and the prefix is non-empty). Used to pin
// the firmware-binary URL to BIN_URL_PREFIX from config.h.
bool bin_url_is_safe(const String& url, const String& prefix);

// True iff `md5` is exactly 32 hexadecimal characters (any case). Empty,
// wrong-length, or non-hex input → false.
bool md5_is_valid(const String& md5);

}  // namespace ota
