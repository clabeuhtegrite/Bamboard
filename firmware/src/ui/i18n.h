// Bamboard UI internationalisation.
//
// Every translatable UI string has an id in `Str`; tr(id) returns it in the
// currently-selected language. The language is chosen in the captive portal
// (NVS key `lang`) and applied at boot via set_language().
//
// Five languages, all Latin-script so they render with the bb_font_* fonts
// (Montserrat + Latin-1). Order matters — it's the on-wire NVS index.
//
// NOT translated (kept verbatim): brand/acronym terms (Bamboard, Bambuddy,
// AMS, HMS), units (°C, %, mm, ms), and the Bambu speed-mode names
// (Silent / Standard / Sport / Ludicrous).

#pragma once

#include <stdint.h>

namespace i18n {

enum class Lang : uint8_t { EN = 0, ES = 1, FR = 2, PT = 3, DE = 4, COUNT = 5 };

enum class Str : uint16_t {
    // tabs / screen titles
    TAB_LIVE, TAB_PRINTERS, TAB_QUEUE, TAB_HISTORY, TAB_SETTINGS,
    // printer states (see theme.cpp state_name)
    STATE_IDLE, STATE_PREPARE, STATE_PRINTING, STATE_PAUSED, STATE_FINISH,
    STATE_FAILED, STATE_OFFLINE, STATE_ERROR, STATE_UNKNOWN,
    // dashboard
    NOZZLE, BED, CHAMBER, SPEED, ETA, ETA_NONE, NO_PRINTER,
    CLEAR_PLATE, CLEAR_HMS, HMS_PREFIX,
    // print controls
    PAUSE, RESUME, CONFIRM_STOP, LIGHT,
    // contextual toasts
    PLATE_CLEARED, CLEAR_PLATE_FAILED, HMS_CLEARED, CLEAR_HMS_FAILED,
    SPEED_CHANGE_FAILED, DRYING_STOPPED, START_DRYING_FAILED, STOP_DRYING_FAILED,
    CONTROL_FAILED, PRINT_DONE, PRINT_FAILED,
    // AMS
    DRY, STOP, EMPTY, NO_AMS,
    // printers
    ADD_IN_BAMBUDDY,
    // history
    PRINTS, SUCCESS, FILAMENT, TIME, NO_PRINTS,
    // settings
    BRIGHTNESS, BRIGHTNESS_FMT, FIRMWARE, LOCAL_IP, WIFI_RSSI, UPTIME, SERVER,
    FACTORY_RESET, CONFIRM_RESET, WIFI_SETUP, WIFI_OPENING, LANGUAGE,
    // overlays
    UPDATING_FW, DONT_POWER_OFF, UPDATE_FAILED, HMS_ERROR, CHECK_PRINTER,
    TAP_DISMISS,
    // header
    OFFLINE_SHORT,
    // misc placeholders / status text
    LAYER, NO_FILE, IDLE_PAREN, NO_PRINTERS_FOUND, RESETTING, NOT_CONFIGURED,
    DRYING_STARTED,
    // print queue
    QUEUE_EMPTY, QUEUE_POS,
    // ambient idle clock
    AMBIENT_ALL_IDLE, AMBIENT_NEXT,
    _COUNT
};

// Set / get the active language (index into Lang). Out-of-range is clamped.
void    set_language(uint8_t lang);
uint8_t language();

// Two-letter code <-> index helpers for the captive portal / NVS.
const char* lang_code(uint8_t lang);     // "en","es","fr","pt","de"
const char* lang_name(uint8_t lang);     // "English","Español",… (self-name)
int         lang_from_code(const char* code);  // -1 if unknown

// The string `s` in the active language.
const char* tr(Str s);

}  // namespace i18n
