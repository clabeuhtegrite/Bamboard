// Bambuddy client + WebSocket stubs for the desktop simulator.
//
// Produces deterministic, varied canned data so every screen has
// something realistic to render: a printing X1C, an idle A1, a paused
// P1S, and a finished printer that still needs its plate cleared.
//
// All write calls (clear_hms, clear_plate, set_print_speed, ...) succeed
// silently and log to stderr so you can spot which button you tapped.

#include "../../firmware/src/net/bambuddy_client.h"
#include "../../firmware/src/net/bambuddy_ws.h"

#include <cstdio>
#include <cstring>

namespace bambuddy {

Client   g_client;
WsClient g_ws;

// ---------- Canned data ----------------------------------------------------

static void make_canned(Printer* p, uint8_t count) {
    auto set_type = [](AmsSlot& s, const char* t) {
        std::strncpy(s.type, t, sizeof(s.type) - 1);
        s.type[sizeof(s.type) - 1] = '\0';
    };

    // [0] Workshop X1C — printing a benchy, 62 % done.
    if (count > 0) {
        Printer& x1c = p[0];
        x1c.id = 1;  x1c.name = "Workshop X1C"; x1c.model = "X1 Carbon";
        x1c.state = PrinterState::Printing;
        x1c.progress = 62; x1c.remaining_s = 60 * 72;
        x1c.current_layer = 164; x1c.total_layers = 267;
        x1c.temps = { 220.0f, 60.0f, 35.0f };
        x1c.hms = "ok";
        x1c.filename = "benchy_v2_pla.3mf";
        x1c.speed_level = 2;
        x1c.ams_exists = true;
        x1c.ams_count  = 1;
        AmsUnit& u = x1c.ams[0];
        u = AmsUnit{};
        u.id = 0; u.present = true; u.humidity = 28; u.temp = 29.0f;
        u.slot_count = 4;
        u.slots[0] = {0, true, 0x00A0E9, {}, 62}; set_type(u.slots[0], "PLA");
        u.slots[1] = {1, true, 0x39D98A, {}, 88}; set_type(u.slots[1], "PETG");
        u.slots[2] = {2, false, 0,        {},  0};
        u.slots[3] = {3, true, 0xF5A524, {}, 12}; set_type(u.slots[3], "TPU");
    }
    // [1] Bedroom A1 mini — idle, AMS Lite (1 slot).
    if (count > 1) {
        Printer& a1 = p[1];
        a1.id = 2;  a1.name = "Bedroom A1 mini"; a1.model = "A1 mini";
        a1.state = PrinterState::Idle;
        a1.temps = { 25.0f, 23.0f, 23.0f };
        a1.hms = "ok";
        a1.speed_level = 2;
        a1.ams_exists = false;
    }
    // [2] Lab P1S — paused with an HMS warning.
    if (count > 2) {
        Printer& p1s = p[2];
        p1s.id = 3;  p1s.name = "Lab P1S"; p1s.model = "P1S";
        p1s.state = PrinterState::Paused;
        p1s.progress = 17;
        p1s.temps = { 195.0f, 55.0f, 28.0f };
        p1s.hms = "HMS_0300_1000_0001_0001: Door is open during print.";
        p1s.filename = "phone_stand_v3.3mf";
        p1s.speed_level = 1;
        p1s.ams_exists = false;
    }
    // [3] Garage X1E — finished, plate not cleared yet.
    if (count > 3) {
        Printer& x1e = p[3];
        x1e.id = 4;  x1e.name = "Garage X1E"; x1e.model = "X1 Carbon";
        x1e.state = PrinterState::Finish;
        x1e.progress = 100;
        x1e.temps = { 55.0f, 35.0f, 32.0f };
        x1e.hms = "ok";
        x1e.filename = "voron_z_idler.3mf";
        x1e.speed_level = 2;
        x1e.ams_exists = false;
    }
}

// ---------- Client public surface -----------------------------------------

void Client::begin(const String&, const String&) {}
void Client::set_credentials(const String&, const String&) {}

bool Client::fetch_printers()                 { return true; }
bool Client::fetch_printer_status(int)        { return true; }
bool Client::fetch_statistics()               { return true; }
bool Client::fetch_recent_archives(uint8_t)   { return true; }
bool Client::ping_health(uint32_t* lat)       { if (lat) *lat = 23; return true; }

bool Client::refresh_status(int id) {
    std::fprintf(stderr, "[sim] refresh_status(%d)\n", id);
    return true;
}
bool Client::set_print_speed(int id, uint8_t mode) {
    std::fprintf(stderr, "[sim] set_print_speed(%d, mode=%u)\n", id, (unsigned)mode);
    return true;
}
bool Client::clear_hms(int id) {
    std::fprintf(stderr, "[sim] clear_hms(%d)\n", id);
    return true;
}
bool Client::clear_plate(int id) {
    std::fprintf(stderr, "[sim] clear_plate(%d)\n", id);
    return true;
}

bool Client::apply_status_payload(int, JsonVariantConst) {
    return true;
}

void Client::snapshot_printers(Printer* out, uint8_t& count) const {
    constexpr uint8_t kCount = 4;
    count = kCount;
    make_canned(out, kCount);
}

Stats Client::snapshot_stats() const {
    Stats s;
    s.total_prints      = 1234;
    s.successful_prints = 1100;
    s.failed_prints     = 134;
    s.success_rate      = 89.1f;
    s.total_time_s      = 100 * 3600;
    s.total_filament_g  = 15000.0f;
    return s;
}

void Client::snapshot_recent(Archive* out, uint8_t& count) const {
    static const Archive kArchives[] = {
        // status   duration_s   filament_g
        {"benchy_v2_pla.3mf",        "success", 135 * 60, 45.0f},
        {"voron_z_idler.3mf",        "success",  42 * 60, 18.0f},
        {"phone_stand_v3.3mf",       "failed",    7 * 60,  3.0f},
        {"cable_clip_x12.3mf",       "success",  88 * 60, 26.0f},
        {"gridfinity_bin_2x1.3mf",   "stopped",  21 * 60,  9.0f},
    };
    count = sizeof(kArchives) / sizeof(kArchives[0]);
    for (uint8_t i = 0; i < count; ++i) out[i] = kArchives[i];
}

bool Client::last_error(String& out) const {
    out = "";
    return false;
}

// Private members the header declares but the sim doesn't need — provide
// trivial bodies for the linker.
bool Client::do_get (const String&, JsonDocument&)               { return true; }
bool Client::do_post(const String&, const String&, JsonDocument*) { return true; }
PrinterState Client::parse_state(const char*)                   { return PrinterState::Unknown; }

// ---------- WsClient (no-op, always reports connected) --------------------

void WsClient::begin(const String&, const String&) {}
void WsClient::set_credentials(const String&, const String&) {}
void WsClient::loop() {}

// Private helpers the header lists — never invoked by sim code, but the
// linker still wants their definitions.
void WsClient::apply_url(const String&) {}
void WsClient::handle_event(WStype_t, uint8_t*, size_t) {}
void WsClient::handle_text(uint8_t*, size_t) {}

}  // namespace bambuddy

// WsClient::is_connected() is inline-defined in the header, so we don't
// need to provide a body here. Returning true keeps the live "WS push"
// indicator green in the sim.
