#include "bambuddy_client.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "../config.h"

namespace bambuddy {

Client g_client;

// Bambuddy returns tray colours as 8-hex RRGGBBAA (e.g. "FF6A00FF"). LVGL only
// wants RGB, and an empty tray comes back as "" or all-zero — both map to 0.
static uint32_t parse_tray_color(const char* hex) {
    if (!hex || strlen(hex) < 6) return 0;
    char buf[7];
    memcpy(buf, hex, 6);
    buf[6] = '\0';
    return (uint32_t)strtoul(buf, nullptr, 16);
}

static void copy_short(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

PrinterState Client::parse_state(const char* s) {
    if (!s || !*s) return PrinterState::Unknown;
    // Lower-case match without pulling in <strings.h>
    if (!strcasecmp(s, "idle"))     return PrinterState::Idle;
    if (!strcasecmp(s, "prepare"))  return PrinterState::Prepare;
    if (!strcasecmp(s, "printing")) return PrinterState::Printing;
    if (!strcasecmp(s, "paused"))   return PrinterState::Paused;
    if (!strcasecmp(s, "finish"))   return PrinterState::Finish;
    if (!strcasecmp(s, "failed"))   return PrinterState::Failed;
    if (!strcasecmp(s, "offline"))  return PrinterState::Offline;
    if (!strcasecmp(s, "error"))    return PrinterState::Error;
    return PrinterState::Unknown;
}

// --- lifecycle --------------------------------------------------------------

void Client::begin(const String& base_url, const String& api_key) {
    if (!mtx_) mtx_ = xSemaphoreCreateMutex();
    set_credentials(base_url, api_key);
}

void Client::set_credentials(const String& base_url, const String& api_key) {
    if (!mtx_) mtx_ = xSemaphoreCreateMutex();
    xSemaphoreTake(mtx_, portMAX_DELAY);
    base_url_ = base_url;
    // Strip trailing slash so we can append "/api/v1/..."
    while (base_url_.endsWith("/")) base_url_.remove(base_url_.length() - 1);
    api_key_ = api_key;
    xSemaphoreGive(mtx_);
}

// --- HTTP plumbing ----------------------------------------------------------

bool Client::do_get(const String& path, JsonDocument& out_doc) {
    if (!is_configured()) {
        last_error_ = "not configured";
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        last_error_ = "wifi down";
        return false;
    }
    HTTPClient http;
    http.setConnectTimeout(::bambuddy::CONNECT_TIMEOUT_MS);
    http.setTimeout(::bambuddy::READ_TIMEOUT_MS);

    String full = base_url_ + path;
    if (!http.begin(full)) {
        last_error_ = "begin() failed";
        return false;
    }
    http.addHeader("X-API-Key", api_key_);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code <= 0) {
        last_error_ = String("GET ") + path + " failed: " + http.errorToString(code);
        http.end();
        return false;
    }
    if (code < 200 || code >= 300) {
        last_error_ = String("HTTP ") + code + " on " + path;
        http.end();
        return false;
    }
    DeserializationError err = deserializeJson(out_doc, http.getStream());
    http.end();
    if (err) {
        last_error_ = String("JSON: ") + err.c_str();
        return false;
    }
    return true;
}

bool Client::do_post(const String& path, const String& body, JsonDocument* out_doc) {
    if (!is_configured() || WiFi.status() != WL_CONNECTED) {
        last_error_ = "not ready";
        return false;
    }
    HTTPClient http;
    http.setConnectTimeout(::bambuddy::CONNECT_TIMEOUT_MS);
    http.setTimeout(::bambuddy::READ_TIMEOUT_MS);

    if (!http.begin(base_url_ + path)) {
        last_error_ = "begin() failed";
        return false;
    }
    http.addHeader("X-API-Key", api_key_);
    http.addHeader("Content-Type", "application/json");

    int code = body.length() ? http.POST(body) : http.POST((uint8_t*)nullptr, 0);
    if (code <= 0) {
        last_error_ = String("POST ") + path + ": " + http.errorToString(code);
        http.end();
        return false;
    }
    if (code < 200 || code >= 300) {
        last_error_ = String("HTTP ") + code + " on " + path;
        http.end();
        return false;
    }
    if (out_doc) {
        DeserializationError err = deserializeJson(*out_doc, http.getStream());
        if (err) {
            last_error_ = String("JSON: ") + err.c_str();
            http.end();
            return false;
        }
    }
    http.end();
    return true;
}

// --- Read operations --------------------------------------------------------

bool Client::fetch_printers() {
    JsonDocument doc;
    if (!do_get("/api/v1/printers", doc)) return false;
    if (!doc.is<JsonArray>()) {
        last_error_ = "printers: expected array";
        return false;
    }

    xSemaphoreTake(mtx_, portMAX_DELAY);
    JsonArray arr = doc.as<JsonArray>();
    printer_count_ = 0;
    for (JsonObject obj : arr) {
        if (printer_count_ >= MAX_PRINTERS) break;
        Printer& p   = printers_[printer_count_++];
        p.id         = obj["id"]            | -1;
        p.name       = (const char*)(obj["name"]   | "Printer");
        p.model      = (const char*)(obj["model"]  | "");
        p.state      = parse_state(obj["status"]   | "");
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_printer_status(int printer_id) {
    String path = String("/api/v1/printers/") + printer_id + "/status";
    JsonDocument doc;
    if (!do_get(path, doc)) return false;
    return apply_status_payload(printer_id, doc.as<JsonVariantConst>());
}

bool Client::apply_status_payload(int printer_id, JsonVariantConst doc) {
    if (doc.isNull()) return false;
    xSemaphoreTake(mtx_, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < printer_count_; ++i) {
        if (printers_[i].id != printer_id) continue;
        found = true;
        Printer& p       = printers_[i];
        p.state          = parse_state(doc["state"] | "");
        p.progress       = doc["progress"]       | 0;
        p.remaining_s    = doc["remaining_time"] | 0;
        p.current_layer  = doc["current_layer"]  | 0;
        p.total_layers   = doc["total_layers"]   | 0;
        p.temps.nozzle   = doc["temperatures"]["nozzle"]  | 0.0f;
        p.temps.bed      = doc["temperatures"]["bed"]     | 0.0f;
        p.temps.chamber  = doc["temperatures"]["chamber"] | 0.0f;
        p.hms            = (const char*)(doc["hms_status"] | "ok");
        p.filename       = (const char*)(doc["filename"]   | "");

        // --- AMS ---
        p.ams_exists = doc["ams_exists"] | false;
        p.ams_count  = 0;
        JsonArrayConst ams_arr = doc["ams"].as<JsonArrayConst>();
        if (!ams_arr.isNull()) {
            for (JsonObjectConst ams_obj : ams_arr) {
                if (p.ams_count >= 4) break;
                AmsUnit& u = p.ams[p.ams_count++];
                u = AmsUnit{};   // reset slots from any previous snapshot
                u.id         = ams_obj["id"]       | -1;
                u.is_ht      = ams_obj["is_ams_ht"]| false;
                // humidity can legitimately be 0; treat null/missing as -1.
                if (ams_obj["humidity"].is<int>()) u.humidity = (int16_t)ams_obj["humidity"].as<int>();
                else                                u.humidity = -1;
                u.temp         = ams_obj["temp"]     | 0.0f;
                u.dry_time_min = ams_obj["dry_time"] | 0u;
                u.present      = true;

                JsonArrayConst trays = ams_obj["tray"].as<JsonArrayConst>();
                if (!trays.isNull()) {
                    for (JsonObjectConst t : trays) {
                        if (u.slot_count >= 4) break;
                        AmsSlot& s = u.slots[u.slot_count++];
                        s.id        = t["id"] | 0;
                        const char* col = t["tray_color"] | "";
                        s.color_rgb = parse_tray_color(col);
                        copy_short(s.type, sizeof(s.type), t["tray_type"] | "");
                        s.remain    = t["remain"] | 0;
                        // Bambuddy reports empty slots with no tray_type and remain=0.
                        s.present   = (s.type[0] != '\0') || (s.color_rgb != 0) || (s.remain > 0);
                    }
                }
            }
        }
        break;
    }
    xSemaphoreGive(mtx_);
    return found;
}

bool Client::fetch_statistics() {
    JsonDocument doc;
    if (!do_get("/api/v1/statistics", doc)) return false;
    xSemaphoreTake(mtx_, portMAX_DELAY);
    stats_.total_prints      = doc["total_prints"]        | 0;
    stats_.successful_prints = doc["successful_prints"]   | 0;
    stats_.failed_prints     = doc["failed_prints"]       | 0;
    stats_.stopped_prints    = doc["stopped_prints"]      | 0;
    stats_.success_rate      = doc["success_rate"]        | 0.0f;
    stats_.total_time_s      = doc["total_print_time"]    | 0;
    stats_.total_filament_g  = doc["total_filament_used"] | 0.0f;
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_recent_archives(uint8_t limit) {
    if (limit == 0 || limit > MAX_RECENT_ARCHIVES) limit = MAX_RECENT_ARCHIVES;
    String path = String("/api/v1/archives?limit=") + limit;
    JsonDocument doc;
    if (!do_get(path, doc)) return false;
    JsonArray arr = doc["archives"].as<JsonArray>();
    if (arr.isNull()) {
        last_error_ = "archives: missing 'archives' array";
        return false;
    }
    xSemaphoreTake(mtx_, portMAX_DELAY);
    recent_count_ = 0;
    for (JsonObject obj : arr) {
        if (recent_count_ >= MAX_RECENT_ARCHIVES) break;
        Archive& a    = recent_[recent_count_++];
        a.name        = (const char*)(obj["name"]         | "");
        a.status      = (const char*)(obj["status"]       | "");
        a.duration_s  = obj["duration"]      | 0;
        a.filament_g  = obj["filament_used"] | 0.0f;
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::ping_health(uint32_t* latency_ms_out) {
    uint32_t t0 = millis();
    JsonDocument doc;
    bool ok = do_get("/health", doc);
    if (latency_ms_out) *latency_ms_out = millis() - t0;
    return ok;
}

// --- Write operations -------------------------------------------------------

bool Client::refresh_status(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/refresh-status", "", nullptr);
}

bool Client::set_print_speed(int printer_id, uint8_t mode) {
    if (mode < 1 || mode > 4) return false;
    String path = String("/api/v1/printers/") + printer_id + "/print-speed?mode=" + mode;
    return do_post(path, "", nullptr);
}

bool Client::clear_hms(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/hms/clear", "", nullptr);
}

bool Client::clear_plate(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/clear-plate", "", nullptr);
}

// --- Snapshots --------------------------------------------------------------

void Client::snapshot_printers(Printer* out, uint8_t& count) const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    count = printer_count_;
    for (uint8_t i = 0; i < printer_count_; ++i) out[i] = printers_[i];
    xSemaphoreGive(mtx_);
}

Stats Client::snapshot_stats() const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    Stats s = stats_;
    xSemaphoreGive(mtx_);
    return s;
}

void Client::snapshot_recent(Archive* out, uint8_t& count) const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    count = recent_count_;
    for (uint8_t i = 0; i < recent_count_; ++i) out[i] = recent_[i];
    xSemaphoreGive(mtx_);
}

bool Client::last_error(String& out) const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    out = last_error_;
    xSemaphoreGive(mtx_);
    return out.length() > 0;
}

}  // namespace bambuddy
