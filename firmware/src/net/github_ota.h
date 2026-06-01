// GitHub-Releases OTA updater.
//
// Replaces the old LAN push (ArduinoOTA + scripts/update-*). At boot the
// device pulls a manifest from this repo's *latest* GitHub Release, compares
// its version against the running firmware, and — if a newer release exists —
// downloads and flashes the .bin straight from the release asset. The update
// is mandatory: it runs before the rest of the app comes up.
//
// Everything here is blocking and meant to be called once from setup() after
// Wi-Fi is connected. On success the device reboots inside check_and_update()
// and control never returns. On any failure (offline, GitHub down, bad
// manifest, flash error) it returns and boot continues on the current build,
// so a network hiccup can never brick a device.

#pragma once

#include <Arduino.h>

#include "semver.h"   // semver_cmp() lives here so it's unit-testable on the host

namespace ota {

enum class CheckResult : uint8_t {
    UpToDate,    // running version >= latest published release
    NoNetwork,   // Wi-Fi down, or the manifest couldn't be fetched
    BadManifest, // manifest fetched but unparseable / missing fields
    Failed,      // a newer version was found but the download/flash failed
};

// on_start()      fires once, the moment a newer release is confirmed and the
//                 download is about to begin (use it to show the OTA overlay).
// on_progress(p)  fires repeatedly during the download with 0..100.
// Either callback may be null.
CheckResult check_and_update(void (*on_start)() = nullptr,
                             void (*on_progress)(uint8_t) = nullptr);

}  // namespace ota
