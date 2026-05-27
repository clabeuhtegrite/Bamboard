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

    // --- Write operations (require printers:control on the API key) ---
    bool refresh_status(int printer_id);
    bool set_print_speed(int printer_id, uint8_t mode);  // 1..4
    bool clear_hms(int printer_id);
    bool clear_plate(int printer_id);

    // --- Snapshots (cheap; safe to call from the LVGL task) ---
    void snapshot_printers(Printer* out, uint8_t& count) const;
    Stats snapshot_stats() const;
    void snapshot_recent(Archive* out, uint8_t& count) const;
    bool last_error(String& out) const;

   private:
    bool do_get(const String& path, JsonDocument& out_doc);
    bool do_post(const String& path, const String& body, JsonDocument* out_doc);

    static PrinterState parse_state(const char* s);

    String base_url_;
    String api_key_;

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
