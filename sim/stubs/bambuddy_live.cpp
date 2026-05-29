// =============================================================================
// Real Bambuddy backend for the desktop simulator.
// =============================================================================
//
// Built only when the sim is configured with -DBAMBOARD_SIM_LIVE=ON. Same
// public API as the canned stubs in sim/stubs/bambuddy_stubs.cpp — every
// UI button still routes to bambuddy::g_client.set_print_speed /
// clear_plate / clear_hms / start_ams_drying — but instead of printing
// "[sim] set_print_speed(1, mode=3)" to stderr we actually POST to the
// real server.
//
// Configuration comes from the environment at launch time:
//   BAMBUDDY_URL = "http://192.168.1.42:8000"
//   BAMBUDDY_KEY = "<your-bambuddy-api-key>"
// Either missing → log a warning, serve empty data, every screen stays
// reachable but renders as if no printer were attached.
//
// HTTPS is intentionally out of scope for v1.1: self-hosted Bambuddy is
// almost always plain HTTP on the LAN. Drop in OpenSSL and rebuild
// cpp-httplib with CPPHTTPLIB_OPENSSL_SUPPORT if you need TLS.
//
// Real WebSocket push is also out of scope. The firmware already
// gracefully falls back to REST polling when WS is down — we just keep
// reporting "not connected" from WsClient so the poller stays on the
// 2-second cadence (firmware's POLL_DASHBOARD_MS).

#include "../../firmware/src/net/bambuddy_client.h"
#include "../../firmware/src/net/bambuddy_ws.h"

#define CPPHTTPLIB_USE_POLL 1   // avoid select()'s FD limit on Windows
#include <httplib.h>
#include <ArduinoJson.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace bambuddy {

Client   g_client;
WsClient g_ws;

// =============================================================================
// Live state — populated by the poller thread, read by the UI thread.
// =============================================================================

namespace {

std::mutex          s_mtx;
std::atomic<bool>   s_alive{false};
std::thread         s_poller;

std::string         s_base_url;    // e.g. "http://192.168.1.42:8000"
std::string         s_api_key;

Printer             s_printers[8];
uint8_t             s_printer_count = 0;
Stats               s_stats;
Archive             s_recent[10];
uint8_t             s_recent_count = 0;
std::string         s_last_error;

// Read BAMBUDDY_URL / BAMBUDDY_KEY out of env. Returns false if either
// is missing — the caller falls back to "serve empty" mode.
bool load_creds() {
    const char* url = std::getenv("BAMBUDDY_URL");
    const char* key = std::getenv("BAMBUDDY_KEY");
    if (!url || !*url || !key || !*key) {
        std::fprintf(stderr,
            "[sim/live] BAMBUDDY_URL or BAMBUDDY_KEY not set in the environment.\n"
            "[sim/live] Set both before launching the sim, e.g.\n"
            "[sim/live]   set BAMBUDDY_URL=http://192.168.1.42:8000   (Windows)\n"
            "[sim/live]   set BAMBUDDY_KEY=your-api-key\n"
            "[sim/live] or\n"
            "[sim/live]   export BAMBUDDY_URL=http://192.168.1.42:8000  (macOS/Linux)\n"
            "[sim/live]   export BAMBUDDY_KEY=your-api-key\n"
            "[sim/live] Serving empty data for now.\n");
        return false;
    }
    s_base_url = url;
    s_api_key  = key;
    while (!s_base_url.empty() && s_base_url.back() == '/')
        s_base_url.pop_back();
    std::fprintf(stderr, "[sim/live] Polling %s every 2 s\n", s_base_url.c_str());
    return true;
}

// One cpp-httplib client per call. Setting up + tearing down the TCP
// socket every 2 s is plenty cheap for a single-printer Bambuddy and
// keeps the lifetime story trivial (no thread-safety to worry about
// since each request owns its own client).
httplib::Client make_http() {
    httplib::Client c(s_base_url);
    c.set_connection_timeout(4, 0);    // 4 s
    c.set_read_timeout(6, 0);          // 6 s
    c.set_default_headers({
        {"X-API-Key", s_api_key},
        {"Accept",    "application/json"},
    });
    return c;
}

// ---- JSON parsing helpers (mirrors firmware/src/net/bambuddy_client.cpp) ----

PrinterState parse_state_str(const char* s) {
    if (!s || !*s) return PrinterState::Unknown;
#ifdef _WIN32
    auto eq = [](const char* a, const char* b) { return _stricmp(a, b) == 0; };
#else
    auto eq = [](const char* a, const char* b) { return strcasecmp(a, b) == 0; };
#endif
    if (eq(s, "idle"))     return PrinterState::Idle;
    if (eq(s, "prepare"))  return PrinterState::Prepare;
    if (eq(s, "printing")) return PrinterState::Printing;
    if (eq(s, "paused"))   return PrinterState::Paused;
    if (eq(s, "finish"))   return PrinterState::Finish;
    if (eq(s, "failed"))   return PrinterState::Failed;
    if (eq(s, "offline"))  return PrinterState::Offline;
    if (eq(s, "error"))    return PrinterState::Error;
    return PrinterState::Unknown;
}

uint32_t parse_tray_color(const char* hex) {
    if (!hex || std::strlen(hex) < 6) return 0;
    char buf[7];
    std::memcpy(buf, hex, 6);
    buf[6] = '\0';
    return (uint32_t)std::strtoul(buf, nullptr, 16);
}

void copy_short(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = std::strlen(src);
    if (n >= cap) n = cap - 1;
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}

// Apply a /status-shaped JSON to a Printer. Called with s_mtx held.
void apply_status(Printer& p, JsonVariantConst doc) {
    p.state          = parse_state_str(doc["state"] | "");
    p.progress       = doc["progress"]       | 0;
    p.remaining_s    = doc["remaining_time"] | 0;
    p.current_layer  = doc["current_layer"]  | 0;
    p.total_layers   = doc["total_layers"]   | 0;
    p.temps.nozzle   = doc["temperatures"]["nozzle"]  | 0.0f;
    p.temps.bed      = doc["temperatures"]["bed"]     | 0.0f;
    p.temps.chamber  = doc["temperatures"]["chamber"] | 0.0f;
    p.hms            = (const char*)(doc["hms_status"] | "ok");
    p.filename       = (const char*)(doc["filename"]   | "");
    p.speed_level    = doc["speed_level"] | 2;

    p.ams_exists = doc["ams_exists"] | false;
    p.ams_count  = 0;
    JsonArrayConst ams_arr = doc["ams"].as<JsonArrayConst>();
    if (!ams_arr.isNull()) {
        for (JsonObjectConst ams_obj : ams_arr) {
            if (p.ams_count >= 4) break;
            AmsUnit& u = p.ams[p.ams_count++];
            u = AmsUnit{};
            u.id      = ams_obj["id"]        | -1;
            u.is_ht   = ams_obj["is_ams_ht"] | false;
            if (ams_obj["humidity"].is<int>())
                u.humidity = (int16_t)ams_obj["humidity"].as<int>();
            else
                u.humidity = -1;
            u.temp         = ams_obj["temp"]     | 0.0f;
            u.dry_time_min = ams_obj["dry_time"] | 0u;
            u.present      = true;
            JsonArrayConst trays = ams_obj["tray"].as<JsonArrayConst>();
            if (!trays.isNull()) {
                for (JsonObjectConst t : trays) {
                    if (u.slot_count >= 4) break;
                    AmsSlot& s = u.slots[u.slot_count++];
                    s.id        = t["id"] | 0;
                    s.color_rgb = parse_tray_color(t["tray_color"] | "");
                    copy_short(s.type, sizeof(s.type), t["tray_type"] | "");
                    s.remain    = t["remain"] | 0;
                    s.present   = (s.type[0] != '\0') ||
                                  (s.color_rgb != 0) ||
                                  (s.remain > 0);
                }
            }
        }
    }
}

void set_error(const std::string& msg) {
    std::lock_guard<std::mutex> lk(s_mtx);
    s_last_error = msg;
}

// ---- One full poll cycle ----------------------------------------------------

void poll_once() {
    auto c = make_http();

    // 1) /printers ----------------------------------------------------------
    {
        auto res = c.Get("/api/v1/printers");
        if (!res) {
            set_error("connect failed");
            return;
        }
        if (res->status != 200) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "HTTP %d on /printers", res->status);
            set_error(buf);
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, res->body) != DeserializationError::Ok ||
            !doc.is<JsonArray>()) {
            set_error("bad JSON on /printers");
            return;
        }
        std::lock_guard<std::mutex> lk(s_mtx);
        JsonArray arr = doc.as<JsonArray>();
        s_printer_count = 0;
        for (JsonObject obj : arr) {
            if (s_printer_count >= 8) break;
            Printer& p = s_printers[s_printer_count++];
            p.id    = obj["id"]    | -1;
            p.name  = (const char*)(obj["name"]  | "Printer");
            p.model = (const char*)(obj["model"] | "");
            p.state = parse_state_str(obj["status"] | "");
        }
        s_last_error.clear();
    }

    // 2) /printers/:id/status — one fetch per printer ----------------------
    uint8_t n; int ids[8];
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        n = s_printer_count;
        for (uint8_t i = 0; i < n; ++i) ids[i] = s_printers[i].id;
    }
    for (uint8_t i = 0; i < n; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path),
                      "/api/v1/printers/%d/status", ids[i]);
        auto res = c.Get(path);
        if (!res || res->status != 200) continue;
        JsonDocument doc;
        if (deserializeJson(doc, res->body) != DeserializationError::Ok) continue;
        std::lock_guard<std::mutex> lk(s_mtx);
        for (uint8_t k = 0; k < s_printer_count; ++k) {
            if (s_printers[k].id == ids[i]) {
                apply_status(s_printers[k], doc.as<JsonVariantConst>());
                break;
            }
        }
    }

    // 3) /statistics --------------------------------------------------------
    if (auto res = c.Get("/api/v1/statistics"); res && res->status == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, res->body) == DeserializationError::Ok) {
            std::lock_guard<std::mutex> lk(s_mtx);
            s_stats.total_prints      = doc["total_prints"]        | 0;
            s_stats.successful_prints = doc["successful_prints"]   | 0;
            s_stats.failed_prints     = doc["failed_prints"]       | 0;
            s_stats.stopped_prints    = doc["stopped_prints"]      | 0;
            s_stats.success_rate      = doc["success_rate"]        | 0.0f;
            s_stats.total_time_s      = doc["total_print_time"]    | 0;
            s_stats.total_filament_g  = doc["total_filament_used"] | 0.0f;
        }
    }

    // 4) /archives?limit=10 -------------------------------------------------
    if (auto res = c.Get("/api/v1/archives?limit=10"); res && res->status == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, res->body) == DeserializationError::Ok) {
            JsonArray arr = doc["archives"].as<JsonArray>();
            if (!arr.isNull()) {
                std::lock_guard<std::mutex> lk(s_mtx);
                s_recent_count = 0;
                for (JsonObject obj : arr) {
                    if (s_recent_count >= 10) break;
                    Archive& a = s_recent[s_recent_count++];
                    a.name       = (const char*)(obj["name"]   | "");
                    a.status     = (const char*)(obj["status"] | "");
                    a.duration_s = obj["duration"]      | 0;
                    a.filament_g = obj["filament_used"] | 0.0f;
                }
            }
        }
    }
}

void poller_main() {
    // Tiny warm-up: catch the obvious mistake (wrong URL / firewall) and
    // log it once before settling into the 2 s loop.
    poll_once();
    while (s_alive.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (!s_alive.load(std::memory_order_relaxed)) break;
        try {
            poll_once();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[sim/live] poll exception: %s\n", e.what());
        }
    }
}

// ---- Write-path helpers ----------------------------------------------------

bool do_post_path(const std::string& path) {
    if (!s_alive.load()) return false;
    auto c = make_http();
    auto res = c.Post(path.c_str(), "", "application/json");
    return res && res->status >= 200 && res->status < 300;
}

bool do_post_json(const std::string& path, const std::string& body) {
    if (!s_alive.load()) return false;
    auto c = make_http();
    auto res = c.Post(path.c_str(), body, "application/json");
    return res && res->status >= 200 && res->status < 300;
}

}  // anonymous namespace

// =============================================================================
// Client public surface — all read paths return whatever the poller
// last cached; all write paths POST synchronously and return true on
// 2xx. Every call mirrors the canned-stub's stderr trace so you can see
// which button the user tapped against the live server.
// =============================================================================

void Client::begin(const String&, const String&) {
    // First call wins — subsequent ones are no-ops. The sim's UI manager
    // never re-binds creds at runtime (no captive portal in the sim).
    if (s_alive.load()) return;
    if (!load_creds()) return;
    s_alive.store(true);
    s_poller = std::thread(poller_main);
}

void Client::set_credentials(const String&, const String&) {
    // No captive portal in the sim; creds come from env at startup.
}

bool Client::fetch_printers()               { return s_alive.load(); }
bool Client::fetch_printer_status(int)      { return s_alive.load(); }
bool Client::fetch_statistics()             { return s_alive.load(); }
bool Client::fetch_recent_archives(uint8_t) { return s_alive.load(); }

bool Client::ping_health(uint32_t* lat) {
    if (!s_alive.load()) { if (lat) *lat = 0; return false; }
    auto t0  = std::chrono::steady_clock::now();
    auto c   = make_http();
    auto res = c.Get("/health");
    auto dt  = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    if (lat) *lat = (uint32_t)dt;
    return res && res->status == 200;
}

bool Client::refresh_status(int id) {
    char p[64];
    std::snprintf(p, sizeof(p), "/api/v1/printers/%d/refresh-status", id);
    bool ok = do_post_path(p);
    std::fprintf(stderr, "[sim/live] refresh_status(%d) -> %s\n",
                 id, ok ? "OK" : "FAIL");
    return ok;
}

bool Client::set_print_speed(int id, uint8_t mode) {
    if (mode < 1 || mode > 4) return false;
    char p[80];
    std::snprintf(p, sizeof(p),
                  "/api/v1/printers/%d/print-speed?mode=%u",
                  id, (unsigned)mode);
    bool ok = do_post_path(p);
    std::fprintf(stderr, "[sim/live] set_print_speed(%d, mode=%u) -> %s\n",
                 id, (unsigned)mode, ok ? "OK" : "FAIL");
    return ok;
}

bool Client::clear_hms(int id) {
    char p[64];
    std::snprintf(p, sizeof(p), "/api/v1/printers/%d/hms/clear", id);
    bool ok = do_post_path(p);
    std::fprintf(stderr, "[sim/live] clear_hms(%d) -> %s\n",
                 id, ok ? "OK" : "FAIL");
    return ok;
}

bool Client::clear_plate(int id) {
    char p[64];
    std::snprintf(p, sizeof(p), "/api/v1/printers/%d/clear-plate", id);
    bool ok = do_post_path(p);
    std::fprintf(stderr, "[sim/live] clear_plate(%d) -> %s\n",
                 id, ok ? "OK" : "FAIL");
    return ok;
}

bool Client::start_ams_drying(int id, uint8_t unit,
                               uint16_t minutes, uint8_t temp_c) {
    char p[80];
    std::snprintf(p, sizeof(p),
                  "/api/v1/printers/%d/ams/%u/dry", id, (unsigned)unit);
    char body[80];
    std::snprintf(body, sizeof(body),
                  "{\"minutes\":%u,\"temp\":%u}",
                  (unsigned)minutes, (unsigned)temp_c);
    bool ok = do_post_json(p, body);
    std::fprintf(stderr,
                 "[sim/live] start_ams_drying(%d, unit=%u, %u min @ %u C) -> %s\n",
                 id, (unsigned)unit, (unsigned)minutes, (unsigned)temp_c,
                 ok ? "OK" : "FAIL");
    return ok;
}

bool Client::stop_ams_drying(int id, uint8_t unit) {
    char p[80];
    std::snprintf(p, sizeof(p),
                  "/api/v1/printers/%d/ams/%u/dry/stop",
                  id, (unsigned)unit);
    bool ok = do_post_path(p);
    std::fprintf(stderr, "[sim/live] stop_ams_drying(%d, unit=%u) -> %s\n",
                 id, (unsigned)unit, ok ? "OK" : "FAIL");
    return ok;
}

bool Client::apply_status_payload(int, JsonVariantConst) {
    // The live sim doesn't go through this entry point — the poller
    // thread calls apply_status() directly inside this TU. Provide a
    // body so the linker is happy if some other code references it.
    return true;
}

void Client::snapshot_printers(Printer* out, uint8_t& count) const {
    std::lock_guard<std::mutex> lk(s_mtx);
    count = s_printer_count;
    for (uint8_t i = 0; i < s_printer_count; ++i) out[i] = s_printers[i];
}

Stats Client::snapshot_stats() const {
    std::lock_guard<std::mutex> lk(s_mtx);
    return s_stats;
}

void Client::snapshot_recent(Archive* out, uint8_t& count) const {
    std::lock_guard<std::mutex> lk(s_mtx);
    count = s_recent_count;
    for (uint8_t i = 0; i < s_recent_count; ++i) out[i] = s_recent[i];
}

bool Client::last_error(String& out) const {
    std::lock_guard<std::mutex> lk(s_mtx);
    out = s_last_error.c_str();
    return !s_last_error.empty();
}

// Private members the header declares but the live impl doesn't use —
// trivial bodies keep the linker happy.
bool Client::do_get (const String&, JsonDocument&)                { return true; }
bool Client::do_post(const String&, const String&, JsonDocument*) { return true; }
PrinterState Client::parse_state(const char*)                    { return PrinterState::Unknown; }

// =============================================================================
// WsClient — no-op for v1.1. is_connected() returns false (default
// member init in the header) so the REST poller stays on the 2-second
// cadence rather than the 30-second WS-fallback path.
// =============================================================================

void WsClient::begin(const String&, const String&) {}
void WsClient::set_credentials(const String&, const String&) {}
void WsClient::loop() {}
void WsClient::apply_url(const String&) {}
void WsClient::handle_event(WStype_t, uint8_t*, size_t) {}
void WsClient::handle_text(uint8_t*, size_t) {}

}  // namespace bambuddy
