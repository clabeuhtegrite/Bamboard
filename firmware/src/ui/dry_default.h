// Per-filament-type drying defaults — the fallback used when a spool carries no
// RFID drying profile (third-party filament). Pure, dependency-free logic so it
// can be unit-tested on the host (tests/test_logic.cpp); the AMS screen layers
// the RFID-then-fallback-then-generic selection on top (unit_dry_params).
#pragma once

#include <cstdint>

namespace ui {

struct DryDefault {
    const char* prefix;
    uint8_t     temp_c;
    uint8_t     hours;
};

// Prefix-match `type` (the tray_type tag, e.g. "PLA", "PA-CF") against the
// table; returns {nullptr, 0, 0} for an unknown / empty / null type.
DryDefault dry_default_for(const char* type);

}  // namespace ui
