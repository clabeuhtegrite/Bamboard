#include "bambuddy_client.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "../config.h"
#include "psram_json.h"   // JSON docs allocate from PSRAM, not scarce DRAM

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
    // Bambuddy v0.2.x declares the route with a trailing slash; without
    // it FastAPI returns 404 (redirect_slashes=False on this build).
    // /printers/ only carries identity fields (id / name / model / serial)
    // — the runtime state lives on /printers/{id}/status, fetched
    // separately by net_task.
    JsonDocument doc(&psram_json_allocator());
    if (!do_get("/api/v1/printers/", doc)) return false;
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
        // /printers/ doesn't carry state — leave as Unknown until /status fires.
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_printer_status(int printer_id) {
    String path = String("/api/v1/printers/") + printer_id + "/status";
    JsonDocument doc(&psram_json_allocator());
    if (!do_get(path, doc)) return false;
    return apply_status_payload(printer_id, doc.as<JsonVariantConst>());
}

// Pick the first key present on `obj` that holds a numeric value. Bambuddy's
// PrinterStatus.temperatures is a free-form dict mirroring the MQTT payload,
// which has used both `nozzle` and `nozzle_temper` across firmware versions.
// Tolerate both so we don't have to guess from the schema (the openapi.json
// only documents the field as "object").
static float pick_float(JsonVariantConst v, const char* a, const char* b) {
    if (v.isNull()) return 0.0f;
    if (v[a].is<float>() || v[a].is<int>()) return v[a].as<float>();
    if (v[b].is<float>() || v[b].is<int>()) return v[b].as<float>();
    return 0.0f;
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
        // progress can be null (idle printer); ArduinoJson's `| 0` default
        // handles that for us, but the actual field is a float on the wire.
        p.progress       = (uint8_t)(doc["progress"] | 0.0f);
        p.remaining_s    = doc["remaining_time"] | 0;
        // Bambuddy renames Bambu's "current_layer" to "layer_num".
        p.current_layer  = doc["layer_num"]      | 0;
        p.total_layers   = doc["total_layers"]   | 0;
        // temperatures is a free-form dict; try both Bambu naming styles.
        JsonVariantConst t = doc["temperatures"];
        p.temps.nozzle   = pick_float(t, "nozzle",  "nozzle_temper");
        p.temps.bed      = pick_float(t, "bed",     "bed_temper");
        p.temps.chamber  = pick_float(t, "chamber", "chamber_temper");

        // HMS: Bambuddy returns an array of {code, module, severity, attr}.
        // Empty array → "ok"; otherwise stringify the first one's code so the
        // existing UI (which expects a single short string) still works.
        JsonArrayConst hms = doc["hms_errors"].as<JsonArrayConst>();
        if (hms.isNull() || hms.size() == 0) {
            p.hms = "ok";
        } else {
            JsonObjectConst first = hms[0];
            const char* code = first["code"] | "";
            if (hms.size() == 1) {
                p.hms = code;
            } else {
                p.hms = String(code) + " (+" + (uint32_t)(hms.size() - 1) + " more)";
            }
        }
        // Bambu keeps low-severity HMS notifications (AMS / info) in hms_errors
        // even on a perfectly healthy printer — Bambuddy's own UI doesn't flag
        // them. Surfacing them popped a full-screen "HMS ERROR" alert on a
        // finished, fault-free print. Only treat HMS as active (inline line +
        // Clear-HMS action + full-screen flash all key off p.hms) when the
        // printer is genuinely in a fault/paused state.
        if (p.state != PrinterState::Failed &&
            p.state != PrinterState::Error &&
            p.state != PrinterState::Paused) {
            p.hms = "ok";
        }

        // Filename: Bambuddy exposes the on-printer name as `subtask_name`
        // first, falling back to the raw `gcode_file` path.
        const char* fn = doc["subtask_name"] | "";
        if (!*fn) fn = doc["gcode_file"] | "";
        p.filename    = fn;
        p.speed_level = doc["speed_level"] | 2;

        // --- AMS ---
        p.ams_exists = doc["ams_exists"] | false;
        p.ams_count  = 0;
        JsonArrayConst ams_arr = doc["ams"].as<JsonArrayConst>();
        if (!ams_arr.isNull()) {
            for (JsonObjectConst ams_obj : ams_arr) {
                if (p.ams_count >= 4) break;
                AmsUnit& u = p.ams[p.ams_count++];
                u = AmsUnit{};
                u.id         = ams_obj["id"]        | -1;
                u.is_ht      = ams_obj["is_ams_ht"] | false;
                if (ams_obj["humidity"].is<int>()) u.humidity = (int16_t)ams_obj["humidity"].as<int>();
                else                                u.humidity = -1;
                u.temp         = ams_obj["temp"]     | 0.0f;
                u.dry_time_min = ams_obj["dry_time"] | 0u;
                u.present      = true;

                JsonArrayConst trays = ams_obj["tray"].as<JsonArrayConst>();
                if (!trays.isNull()) {
                    for (JsonObjectConst tr : trays) {
                        if (u.slot_count >= 4) break;
                        AmsSlot& s = u.slots[u.slot_count++];
                        s.id        = tr["id"] | 0;
                        const char* col = tr["tray_color"] | "";
                        s.color_rgb = parse_tray_color(col);
                        copy_short(s.type, sizeof(s.type), tr["tray_type"] | "");
                        s.remain    = tr["remain"] | 0;
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
    // Bambuddy 0.2.x exposes archive stats under /archives/stats, not
    // /statistics. Field names changed too: total_print_time_hours
    // replaces total_print_time, total_filament_grams replaces
    // total_filament_used, and success_rate isn't returned (we compute
    // it from the counts).
    JsonDocument doc(&psram_json_allocator());
    if (!do_get("/api/v1/archives/stats", doc)) return false;
    xSemaphoreTake(mtx_, portMAX_DELAY);
    stats_.total_prints      = doc["total_prints"]      | 0;
    stats_.successful_prints = doc["successful_prints"] | 0;
    stats_.failed_prints     = doc["failed_prints"]     | 0;
    stats_.stopped_prints    = 0;   // not in the new schema
    stats_.total_time_s      = (uint32_t)((doc["total_print_time_hours"] | 0.0f) * 3600.0f);
    stats_.total_filament_g  = doc["total_filament_grams"] | 0.0f;
    stats_.success_rate      = stats_.total_prints > 0
        ? (100.0f * stats_.successful_prints / stats_.total_prints)
        : 0.0f;
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_recent_archives(uint8_t limit) {
    if (limit == 0 || limit > MAX_RECENT_ARCHIVES) limit = MAX_RECENT_ARCHIVES;
    // /archives/slim returns a leaner shape — exactly what we render.
    // Field renames vs. v0.1: name → print_name, duration → actual_time_seconds
    // (falling back to print_time_seconds if the run wasn't tracked),
    // filament_used → filament_used_grams.
    String path = String("/api/v1/archives/slim?limit=") + limit;
    JsonDocument doc(&psram_json_allocator());
    if (!do_get(path, doc)) return false;
    if (!doc.is<JsonArray>()) {
        last_error_ = "archives/slim: expected array";
        return false;
    }
    JsonArray arr = doc.as<JsonArray>();
    xSemaphoreTake(mtx_, portMAX_DELAY);
    recent_count_ = 0;
    for (JsonObject obj : arr) {
        if (recent_count_ >= MAX_RECENT_ARCHIVES) break;
        Archive& a    = recent_[recent_count_++];
        a.name        = (const char*)(obj["print_name"] | "");
        a.status      = (const char*)(obj["status"]     | "");
        uint32_t dur  = obj["actual_time_seconds"] | 0u;
        if (dur == 0) dur = obj["print_time_seconds"] | 0u;
        a.duration_s  = dur;
        a.filament_g  = obj["filament_used_grams"] | 0.0f;
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::ping_health(uint32_t* latency_ms_out) {
    uint32_t t0 = millis();
    JsonDocument doc(&psram_json_allocator());
    bool ok = do_get("/health", doc);
    if (latency_ms_out) *latency_ms_out = millis() - t0;
    return ok;
}

bool Client::fetch_camera_jpeg(int printer_id, uint8_t** out_buf, size_t* out_len) {
    *out_buf = nullptr;
    *out_len = 0;
    if (!is_configured() || WiFi.status() != WL_CONNECTED) {
        last_error_ = "cam: not ready";
        return false;
    }
    HTTPClient http;
    http.setConnectTimeout(::bambuddy::CONNECT_TIMEOUT_MS);
    http.setTimeout(::bambuddy::READ_TIMEOUT_MS);
    String path = base_url_ + "/api/v1/printers/" + printer_id + "/camera/snapshot";
    if (!http.begin(path)) { last_error_ = "cam: begin failed"; return false; }
    http.addHeader("X-API-Key", api_key_);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        last_error_ = String("cam HTTP ") + code;
        http.end();
        return false;
    }
    // Cap the buffer so a misbehaving/huge source can't exhaust PSRAM.
    const size_t CAM_MAX = 256 * 1024;
    int      clen = http.getSize();                 // -1 when chunked
    size_t   cap  = (clen > 0 && (size_t)clen <= CAM_MAX) ? (size_t)clen : CAM_MAX;
    uint8_t* buf  = (uint8_t*)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!buf) { last_error_ = "cam: oom"; http.end(); return false; }

    WiFiClient* stream = http.getStreamPtr();
    size_t   got = 0;
    uint32_t t0  = millis();
    while (http.connected() && got < cap) {
        size_t avail = stream->available();
        if (avail) {
            size_t toread = avail < (cap - got) ? avail : (cap - got);
            int r = stream->readBytes(buf + got, toread);
            if (r > 0) { got += r; t0 = millis(); }
        } else {
            if (clen > 0 && got >= (size_t)clen) break;
            if (millis() - t0 > ::bambuddy::READ_TIMEOUT_MS) break;
            delay(2);
        }
        if (clen > 0 && got >= (size_t)clen) break;
    }
    http.end();
    if (got == 0) { heap_caps_free(buf); last_error_ = "cam: empty"; return false; }
    *out_buf = buf;
    *out_len = got;
    return true;
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

bool Client::start_ams_drying(int printer_id, uint8_t unit_id,
                               uint16_t minutes, uint8_t temp_c) {
    // Bambuddy's real route — params are query string, not JSON body.
    //   POST /api/v1/printers/{id}/drying/start?ams_id=N&duration=M&temp=C
    String path = String("/api/v1/printers/") + printer_id +
                  "/drying/start?ams_id=" + (unsigned)unit_id +
                  "&duration=" + minutes +
                  "&temp="     + (unsigned)temp_c;
    return do_post(path, "", nullptr);
}

bool Client::stop_ams_drying(int printer_id, uint8_t unit_id) {
    String path = String("/api/v1/printers/") + printer_id +
                  "/drying/stop?ams_id=" + (unsigned)unit_id;
    return do_post(path, "", nullptr);
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
