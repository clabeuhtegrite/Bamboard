#include "dry_default.h"

#include <cstring>

namespace ui {

// Prefix-matched against the tray_type tag, so more specific prefixes MUST come
// first (PETG before PET, PPS before PP); "PA" intentionally catches all nylons
// (PA, PAHT, PA-CF…). Bambu's own spools report drying_temp / drying_time over
// RFID and take precedence; this is the fallback for tag-less filament.
static const DryDefault kDryTable[] = {
    {"PETG", 65, 8}, {"PET", 65, 8},
    {"PLA",  55, 8},
    {"TPU",  50, 12},
    {"ABS",  80, 8}, {"ASA", 80, 8},
    {"PA",   80, 12},
    {"PC",   90, 10},
    {"PVA",  45, 12},
    {"HIPS", 70, 8},
    {"PPS",  90, 10}, {"PP", 65, 8},
};

DryDefault dry_default_for(const char* type) {
    if (type && *type)
        for (const auto& d : kDryTable)
            if (strncmp(type, d.prefix, strlen(d.prefix)) == 0) return d;
    return {nullptr, 0, 0};
}

}  // namespace ui
