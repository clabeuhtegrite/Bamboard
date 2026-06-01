// Bambuddy REST client
//
// Wraps the small subset of the Bambuddy API that Bamboard actually uses:
//   GET  /api/v1/printers
//   GET  /api/v1/printers/{id}/status
//   GET  /api/v1/statistics
//   GET  /api/v1/archives?limit=N
//   POST /api/v1/printers/{id}/refresh-status
//   POST /api/v1/printers/{id}/print-speed?mode=N
//   POST /api/v1/printers/{id}/hms/clear
//   POST /api/v1/printers/{id}/clear-plate
//   GET  /health
//
// The client never blocks the LVGL task — calls are issued from a dedicated
// FreeRTOS task and the results are merged into thread-safe snapshots that
// the UI reads.
//
// Reference: https://wiki.bambuddy.cool/reference/api/

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace bambuddy {

// ---------- Data model ----------------------------------------------------

struct Temperatures {
    float nozzle  = 0.0f;
    float bed     = 0.0f;
    float chamber = 0.0f;
};

// One filament slot inside an AMS unit. Bambuddy's /status returns the colour
// as RRGGBBAA hex; we strip alpha at parse time so the UI can pass a 0xRRGGBB
// value straight into LVGL.
struct AmsSlot {
    uint8_t  id          = 0;        // 0..3 within the unit
    bool     present     = false;    // true when the slot reports any filament data
    uint32_t color_rgb   = 0;        // 0xRRGGBB, 0 = unknown / black (see translucent)
    bool     translucent = false;    // tray_color alpha < 0x80 → clear/see-through
    char     type[12]    = {};       // "PLA", "PETG", "" — short tag for the slot
    uint8_t  remain      = 0;        // 0..100, Bambu's RFID estimate
    // RFID-recommended drying params for this spool (0 = unknown / no tag).
    // The UI uses these to pick a per-filament drying setpoint.
    uint8_t  dry_temp_c  = 0;        // °C
    uint8_t  dry_time_h  = 0;        // hours
};

// One AMS (or AMS-HT) unit. Bambu chains up to 4 units per printer; we cap at
// MAX_AMS_UNITS and surface ams_count so the UI can show "AMS 1/2".
struct AmsUnit {
    int8_t   id          = -1;
    bool     present     = false;
    bool     is_ht       = false;    // AMS-HT (1-slot high-temp)
    int16_t  humidity    = -1;       // -1 = unknown, else 0..100 (%)
    float    temp        = 0.0f;     // ambient °C, 0 = unknown
    uint32_t dry_time_min = 0;       // drying countdown in minutes; 0 = idle
    uint8_t  slot_count  = 0;
    AmsSlot  slots[4];
};

enum class PrinterState : uint8_t {
    Unknown,
    Idle,
    Prepare,
    Printing,
    Paused,
    Finish,
    Failed,
    Offline,
    Error,
};

struct Printer {
    int      id            = -1;
    String   name;
    String   model;
    PrinterState state     = PrinterState::Unknown;

    // populated by /status:
    uint8_t  progress      = 0;     // 0..100
    uint32_t remaining_s   = 0;
    uint16_t current_layer = 0;
    uint16_t total_layers  = 0;
    Temperatures temps;
    String   hms;                   // "ok" or short description
    String   filename;
    uint8_t  speed_level = 2;       // 1=Silent, 2=Standard, 3=Sport, 4=Ludicrous

    // populated by /status (ams subtree):
    bool     ams_exists = false;
    uint8_t  ams_count  = 0;
    AmsUnit  ams[4];                // up to 4 chained units; cap matches Bambu
};

struct Stats {
    uint32_t total_prints      = 0;
    uint32_t successful_prints = 0;
    uint32_t failed_prints     = 0;
    uint32_t stopped_prints    = 0;
    float    success_rate      = 0.0f;
    uint32_t total_time_s      = 0;
    float    total_filament_g  = 0.0f;
};

struct Archive {
    String  name;
    String  status;            // "success" | "failed" | "stopped"
    uint32_t duration_s   = 0;
    float    filament_g   = 0.0f;
};

// ---------- Client --------------------------------------------------------

class Client {
   public:
    void begin(const String& base_url, const String& api_key);

    // Updates the URL/key at runtime (e.g. after captive-portal config).
    void set_credentials(const String& base_url, const String& api_key);

    bool is_configured() const { return base_url_.length() && api_key_.length(); }

    // --- Read operations ---
    bool fetch_printers();                          // → printers_
    bool fetch_printer_status(int printer_id);     // → updates that printer
    bool fetch_statistics();                       // → stats_
    bool fetch_recent_archives(uint8_t limit);     // → recent_
    bool ping_health(uint32_t* latency_ms_out);

    // Fetch the latest camera JPEG for a printer into a freshly-allocated PSRAM
    // buffer (caller owns it: free with heap_caps_free). Returns false on any
    // network error or if the response is empty/oversized. Binary, not JSON.
    bool fetch_camera_jpeg(int printer_id, uint8_t** out_buf, size_t* out_len);

    // --- Write operations (require printers:control on the API key) ---
    bool refresh_status(int printer_id);
    bool set_print_speed(int printer_id, uint8_t mode);  // 1..4
    bool clear_hms(int printer_id);
    bool clear_plate(int printer_id);

    // AMS drying control. The exact Bambuddy route hasn't shipped upstream
    // at the time of writing — these issue a POST to
    //   /api/v1/printers/{id}/ams/{unit_id}/dry  (body {"minutes":N,"temp":C})
    //   /api/v1/printers/{id}/ams/{unit_id}/dry/stop
    // and report success/failure to the UI. Once Bambuddy lands an official
    // dryer endpoint we re-point these without UI changes.
    bool start_ams_drying(int printer_id, uint8_t unit_id,
                          uint16_t minutes, uint8_t temp_c);
    bool stop_ams_drying (int printer_id, uint8_t unit_id);

    // Apply a /status-shaped JSON payload to the cached Printer record.
    // Shared between the REST poller and the WebSocket push handler — the
    // ws layer hands us doc["data"] from a printer_status frame, which is
    // produced by Bambuddy's printer_state_to_dict() and therefore matches
    // the same schema this method already parses.
    bool apply_status_payload(int printer_id, JsonVariantConst doc);

    // --- Snapshots (cheap; safe to call from the LVGL task) ---
    void snapshot_printers(Printer* out, uint8_t& count) const;
    Stats snapshot_stats() const;
    void snapshot_recent(Archive* out, uint8_t& count) const;
    bool last_error(String& out) const;

   private:
    bool do_get(const String& path, JsonDocument& out_doc);
    bool do_post(const String& path, const String& body, JsonDocument* out_doc);

    // Fetch + cache a camera stream token (required as ?token= on snapshot/stream).
    bool fetch_camera_token();

    static PrinterState parse_state(const char* s);

    String base_url_;
    String api_key_;

    // Camera stream token (?token= for snapshot/stream). Touched only on the net
    // task, so no mutex. camera_token_ms_ is the millis() it was obtained.
    String   camera_token_;
    uint32_t camera_token_ms_ = 0;

    // Caches — read by the UI task, written by the network task.
    mutable SemaphoreHandle_t mtx_ = nullptr;
    Printer  printers_[8];
    uint8_t  printer_count_ = 0;
    Stats    stats_;
    Archive  recent_[10];
    uint8_t  recent_count_ = 0;
    String   last_error_;
};

extern Client g_client;

}  // namespace bambuddy
