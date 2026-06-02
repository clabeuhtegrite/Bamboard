#include "bambuddy_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../config.h"
#include "ca_bundle.h"    // embedded root-CA bundle (https cert validation)
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

// Alpha byte of an 8-hex RRGGBBAA tray colour. Bambu reports clear / translucent
// filament with a low (usually 00) alpha while solid colours use FF — so the UI
// can tell "transparent PETG" apart from "black PLA" (both arrive as RGB 000000)
// and draw a checkerboard for the see-through one. 6-hex colours carry no alpha
// channel and are treated as opaque.
static uint8_t parse_tray_alpha(const char* hex) {
    if (!hex || strlen(hex) < 8) return 0xFF;
    char buf[3] = { hex[6], hex[7], '\0' };
    return (uint8_t)strtoul(buf, nullptr, 16);
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

void Client::begin(const String& base_url, const String& api_key,
                   const String& cf_id, const String& cf_secret) {
    if (!mtx_) mtx_ = xSemaphoreCreateMutex();
    set_credentials(base_url, api_key, cf_id, cf_secret);
}

void Client::set_credentials(const String& base_url, const String& api_key,
                             const String& cf_id, const String& cf_secret) {
    if (!mtx_) mtx_ = xSemaphoreCreateMutex();
    xSemaphoreTake(mtx_, portMAX_DELAY);
    base_url_ = base_url;
    // Strip trailing slash so we can append "/api/v1/..."
    while (base_url_.endsWith("/")) base_url_.remove(base_url_.length() - 1);
    api_key_   = api_key;
    cf_id_     = cf_id;
    cf_secret_ = cf_secret;
    xSemaphoreGive(mtx_);
}

// Configure an HTTPClient for one request: choose the transport by scheme and
// attach the headers every Bambuddy call shares. For https:// the certificate
// is validated against the embedded root-CA bundle; `secure` is owned by the
// caller so it outlives the request (HTTPClient keeps a reference to it). For
// http:// the bare-WiFiClient path is used exactly as before — `secure` stays
// idle (no TLS context is allocated until a secure client actually connects).
bool Client::begin_request(HTTPClient& http, WiFiClientSecure& secure,
                           const String& url) {
    http.setConnectTimeout(::bambuddy::CONNECT_TIMEOUT_MS);
    http.setTimeout(::bambuddy::READ_TIMEOUT_MS);

    bool ok;
    if (base_url_.startsWith("https://")) {
        secure.setCACertBundle(ca_bundle_start);
        ok = http.begin(secure, url);
    } else {
        ok = http.begin(url);
    }
    if (!ok) return false;

    http.addHeader("X-API-Key", api_key_);
    // Cloudflare Access service token — only ever sent over https so it can't
    // leak onto a clear-text link, even if a stale token were to linger in NVS
    // beside an http:// URL. The portal already blanks it on http; this is the
    // belt-and-braces guard at the point the header is actually emitted.
    if (base_url_.startsWith("https://") && cf_id_.length() && cf_secret_.length()) {
        http.addHeader("CF-Access-Client-Id", cf_id_);
        http.addHeader("CF-Access-Client-Secret", cf_secret_);
    }
    return true;
}

// --- HTTP plumbing ----------------------------------------------------------

bool Client::do_get(const String& path, JsonDocument& out_doc, JsonDocument* filter) {
    if (!is_configured()) {
        last_error_ = "not configured";
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        last_error_ = "wifi down";
        return false;
    }
    // Declare `secure` before `http`: HTTPClient holds a pointer to it and
    // touches it in its destructor, so it must outlive http (destroyed first).
    WiFiClientSecure secure;
    HTTPClient http;
    if (!begin_request(http, secure, base_url_ + path)) {
        last_error_ = "begin() failed";
        return false;
    }
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
    DeserializationError err = filter
        ? deserializeJson(out_doc, http.getStream(), DeserializationOption::Filter(*filter))
        : deserializeJson(out_doc, http.getStream());
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
    WiFiClientSecure secure;   // must outlive http (see do_get)
    HTTPClient http;
    if (!begin_request(http, secure, base_url_ + path)) {
        last_error_ = "begin() failed";
        return false;
    }
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

// Whitelist of the /status fields apply_status_payload actually reads. Bambuddy
// mirrors the entire MQTT report under /status; parsing only these (ArduinoJson
// Filter) keeps the PSRAM doc small and the parse fast. Built once; net-task only.
// MUST stay in sync with the field accesses in apply_status_payload below.
static JsonDocument& status_filter() {
    static JsonDocument f(&psram_json_allocator());
    if (f.isNull()) {
        f["state"]                = true;
        f["progress"]             = true;
        f["remaining_time"]       = true;
        f["layer_num"]            = true;
        f["total_layers"]         = true;
        f["temperatures"]         = true;   // small sub-object; keep it whole
        f["hms_errors"][0]["code"] = true;  // element filter → applies to all
        f["subtask_name"]         = true;
        f["gcode_file"]           = true;
        f["speed_level"]          = true;
        f["chamber_light"]        = true;
        f["cooling_fan_speed"]    = true;
        f["big_fan1_speed"]       = true;
        f["big_fan2_speed"]       = true;
        f["awaiting_plate_clear"] = true;
        f["ams_exists"]           = true;
        JsonObject a = f["ams"][0].to<JsonObject>();
        a["id"] = true; a["is_ams_ht"] = true; a["humidity"] = true;
        a["temp"] = true; a["dry_time"] = true;
        JsonObject tr = a["tray"][0].to<JsonObject>();
        tr["id"] = true; tr["tray_color"] = true; tr["tray_type"] = true;
        tr["remain"] = true; tr["drying_temp"] = true; tr["drying_time"] = true;
    }
    return f;
}

bool Client::fetch_printer_status(int printer_id) {
    String path = String("/api/v1/printers/") + printer_id + "/status";
    JsonDocument doc(&psram_json_allocator());
    if (!do_get(path, doc, &status_filter())) return false;
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
    // Find the printer; if a status arrives for one not yet listed (a WebSocket
    // push before the first /printers fetch, or a host-test fixture), create it
    // so the update isn't dropped.
    Printer* pp = nullptr;
    for (uint8_t i = 0; i < printer_count_; ++i)
        if (printers_[i].id == printer_id) { pp = &printers_[i]; break; }
    if (!pp) {
        if (printer_count_ >= MAX_PRINTERS) { xSemaphoreGive(mtx_); return false; }
        pp = &printers_[printer_count_++];
        *pp = Printer{};
        pp->id = printer_id;
        // Identity normally comes from /printers/; accept it from the payload on
        // the create path only, so an established printer's name isn't churned.
        const char* nm = doc["name"]  | "";  if (*nm) pp->name  = nm;
        const char* md = doc["model"] | "";  if (*md) pp->model = md;
    }
    {
        Printer& p       = *pp;
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

        // Chamber light + fan speeds (int; -1 = unknown). Bambuddy /status keys:
        // chamber_light (bool), cooling_fan_speed (part), big_fan1_speed (aux),
        // big_fan2_speed (chamber/exhaust).
        p.chamber_light = doc["chamber_light"] | false;
        p.fan_cooling   = (int16_t)(doc["cooling_fan_speed"] | -1);
        p.fan_aux       = (int16_t)(doc["big_fan1_speed"]    | -1);
        p.fan_chamber   = (int16_t)(doc["big_fan2_speed"]    | -1);
        // Bambuddy reports whether it's actually waiting for a plate-clear ack
        // separately from the FINISH/FAILED state — this drives the Clear-plate pill.
        p.awaiting_plate_clear = doc["awaiting_plate_clear"] | false;

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
                // Coerce int / float / numeric-string; missing or null → -1
                // (unknown). The old is<int>() check dropped float-encoded values.
                u.humidity = (int16_t)(ams_obj["humidity"] | -1);
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
                        s.color_rgb   = parse_tray_color(col);
                        s.translucent = parse_tray_alpha(col) < 0x80;
                        copy_short(s.type, sizeof(s.type), tr["tray_type"] | "");
                        s.remain    = tr["remain"] | 0;
                        // Bambu's RFID carries the filament's recommended drying
                        // profile (drying_temp °C, drying_time h); the UI uses it
                        // to set a per-filament dry cycle. Clamp before the uint8
                        // cast so an out-of-range/garbage value can't wrap mod 256
                        // into a bogus setpoint we'd push back to the printer.
                        int dtemp = tr["drying_temp"] | 0;
                        int dtime = tr["drying_time"] | 0;
                        s.dry_temp_c = (uint8_t)(dtemp < 0 ? 0 : dtemp > 120 ? 120 : dtemp);
                        s.dry_time_h = (uint8_t)(dtime < 0 ? 0 : dtime > 48  ? 48  : dtime);
                        s.present   = (s.type[0] != '\0') || (s.color_rgb != 0) ||
                                      (s.remain > 0) || s.translucent;
                    }
                }
            }
        }
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::apply_stats_payload(JsonVariantConst doc) {
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

bool Client::fetch_statistics() {
    // Bambuddy 0.2.x exposes archive stats under /archives/stats, not
    // /statistics. Field names changed too: total_print_time_hours
    // replaces total_print_time, total_filament_grams replaces
    // total_filament_used, and success_rate isn't returned (we compute
    // it from the counts).
    JsonDocument doc(&psram_json_allocator());
    if (!do_get("/api/v1/archives/stats", doc)) return false;
    return apply_stats_payload(doc.as<JsonVariantConst>());
}

bool Client::apply_recent_payload(JsonVariantConst doc) {
    if (!doc.is<JsonArrayConst>()) {
        last_error_ = "archives/slim: expected array";
        return false;
    }
    JsonArrayConst arr = doc.as<JsonArrayConst>();
    xSemaphoreTake(mtx_, portMAX_DELAY);
    recent_count_ = 0;
    for (JsonObjectConst obj : arr) {
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

bool Client::fetch_recent_archives(uint8_t limit) {
    if (limit == 0 || limit > MAX_RECENT_ARCHIVES) limit = MAX_RECENT_ARCHIVES;
    // /archives/slim returns a leaner shape — exactly what we render.
    // Field renames vs. v0.1: name → print_name, duration → actual_time_seconds
    // (falling back to print_time_seconds if the run wasn't tracked),
    // filament_used → filament_used_grams.
    String path = String("/api/v1/archives/slim?limit=") + limit;
    JsonDocument doc(&psram_json_allocator());
    if (!do_get(path, doc)) return false;
    return apply_recent_payload(doc.as<JsonVariantConst>());
}

bool Client::apply_queue_payload(JsonVariantConst doc) {
    if (!doc.is<JsonArrayConst>()) {
        last_error_ = "queue: expected array";
        return false;
    }
    JsonArrayConst arr = doc.as<JsonArrayConst>();
    xSemaphoreTake(mtx_, portMAX_DELAY);
    queue_count_ = 0;
    for (JsonObjectConst obj : arr) {
        const char* st = obj["status"] | "";
        if (strcmp(st, "pending") != 0) continue;
        if (queue_count_ >= MAX_QUEUE_ITEMS) break;
        QueueItem& q = queue_[queue_count_++];
        q.id         = obj["id"] | -1;
        q.name       = (const char*)(obj["archive_name"] | "");
        if (q.name.length() == 0) q.name = "(file)";
        q.status     = st;
        q.printer_id = obj["printer_id"] | -1;
        q.position   = obj["position"]   | 0;
    }
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_queue() {
    // The queue endpoint also carries completed/cancelled history; a desk
    // monitor only cares about what's still waiting, so keep "pending" items.
    JsonDocument doc(&psram_json_allocator());
    if (!do_get("/api/v1/queue", doc)) return false;
    return apply_queue_payload(doc.as<JsonVariantConst>());
}

bool Client::apply_system_info_payload(JsonVariantConst doc) {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    sysinfo_.version = (const char*)(doc["app"]["version"]              | "");
    sysinfo_.uptime  = (const char*)(doc["system"]["uptime_formatted"] | "");
    xSemaphoreGive(mtx_);
    return true;
}

bool Client::fetch_system_info() {
    JsonDocument doc(&psram_json_allocator());
    if (!do_get("/api/v1/system/info", doc)) return false;
    return apply_system_info_payload(doc.as<JsonVariantConst>());
}

bool Client::ping_health(uint32_t* latency_ms_out) {
    uint32_t t0 = millis();
    JsonDocument doc(&psram_json_allocator());
    bool ok = do_get("/health", doc);
    if (latency_ms_out) *latency_ms_out = millis() - t0;
    return ok;
}

// Obtain + cache a camera stream token. Bambuddy's snapshot/stream routes reject
// the API key alone with 401 — they require ?token=<t> from
// POST /api/v1/printers/camera/stream-token (the token is reusable for ~60 min).
// Net-task only, so camera_token_ needs no mutex.
bool Client::fetch_camera_token() {
    JsonDocument doc(&psram_json_allocator());
    if (!do_post("/api/v1/printers/camera/stream-token", "", &doc)) {
        last_error_ = "cam: token request failed";
        return false;
    }
    const char* tok = doc["token"] | "";
    if (!*tok) { last_error_ = "cam: no token in response"; return false; }
    camera_token_    = tok;
    camera_token_ms_ = millis();
    return true;
}

bool Client::fetch_camera_jpeg(int printer_id, uint8_t** out_buf, size_t* out_len) {
    *out_buf = nullptr;
    *out_len = 0;
    if (!is_configured() || WiFi.status() != WL_CONNECTED) {
        last_error_ = "cam: not ready";
        return false;
    }

    // Two attempts: a cached token may have expired (60 min server TTL). A 401 on
    // the first try drops it and retries once with a fresh token.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (camera_token_.length() == 0 ||
            millis() - camera_token_ms_ > ::bambuddy::CAMERA_TOKEN_TTL_MS) {
            if (!fetch_camera_token()) return false;   // last_error_ set by helper
        }

        WiFiClientSecure secure;   // must outlive http (see do_get)
        HTTPClient http;
        String path = base_url_ + "/api/v1/printers/" + printer_id +
                      "/camera/snapshot?token=" + camera_token_;
        if (!begin_request(http, secure, path)) { last_error_ = "cam: begin failed"; return false; }

        int code = http.GET();
        if (code == 401 && attempt == 0) {   // stale/invalid token → refresh once
            http.end();
            camera_token_ = "";
            continue;
        }
        if (code != HTTP_CODE_OK) {
            last_error_ = String("cam HTTP ") + code;
            // A 401 here is already the retry (the first attempt refreshed the
            // token): drop the rejected token so the next fetch starts clean
            // instead of re-sending a known-bad one.
            if (code == 401) camera_token_ = "";
            http.end();
            return false;
        }

        // Cap the buffer so a misbehaving/huge source can't exhaust PSRAM. A
        // declared length above the cap would be silently truncated — reject the
        // frame instead of handing a half-image to the JPEG decoder.
        const size_t CAM_MAX = 256 * 1024;
        int      clen = http.getSize();                 // -1 when chunked
        if (clen > 0 && (size_t)clen > CAM_MAX) {
            last_error_ = "cam: frame too large";
            http.end();
            return false;
        }
        size_t   cap  = (clen > 0) ? (size_t)clen : CAM_MAX;
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
                // Once bytes have started arriving, a short idle gap means the
                // frame is complete. A chunked response (clen < 0) has no length
                // to hit, so without this the loop would spin until the full read
                // timeout (~seconds) on every frame — throttling the viewer and
                // hogging the net task. Before the first byte, allow the full budget.
                uint32_t idle_budget = (got > 0) ? 250u : (uint32_t)::bambuddy::READ_TIMEOUT_MS;
                if (millis() - t0 > idle_budget) break;
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
    last_error_ = "cam: token rejected";
    return false;
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

bool Client::pause_print(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/print/pause", "", nullptr);
}

bool Client::resume_print(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/print/resume", "", nullptr);
}

bool Client::stop_print(int printer_id) {
    return do_post(String("/api/v1/printers/") + printer_id + "/print/stop", "", nullptr);
}

bool Client::set_chamber_light(int printer_id, bool on) {
    // Bambuddy takes on/off as a required query param, not a JSON body.
    String path = String("/api/v1/printers/") + printer_id +
                  "/chamber-light?on=" + (on ? "true" : "false");
    return do_post(path, "", nullptr);
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

bool Client::cancel_queue_item(int item_id) {
    return do_post(String("/api/v1/queue/") + item_id + "/cancel", "", nullptr);
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

void Client::snapshot_queue(QueueItem* out, uint8_t& count) const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    count = queue_count_;
    for (uint8_t i = 0; i < queue_count_; ++i) out[i] = queue_[i];
    xSemaphoreGive(mtx_);
}

SystemInfo Client::snapshot_system_info() const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    SystemInfo s = sysinfo_;
    xSemaphoreGive(mtx_);
    return s;
}

bool Client::last_error(String& out) const {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    out = last_error_;
    xSemaphoreGive(mtx_);
    return out.length() > 0;
}

}  // namespace bambuddy
