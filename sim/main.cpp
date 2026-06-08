// Bamboard host simulator harness.
//
// Boots LVGL against a memory framebuffer, points the REAL Bambuddy REST client
// at the user's real server (URL/key + Cloudflare Access token from the env),
// fetches real data, then renders each screen to a PNG. No fabricated data: if
// the server is unreachable the screens simply render their empty states.
//
// Env: BAMBUDDY_URL, BAMBUDDY_API_KEY, CF_ACCESS_CLIENT_ID,
//      CF_ACCESS_CLIENT_SECRET, BAMBOARD_LANG (en/es/fr/pt/de), SIM_OUT (dir).

#include <lvgl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "config.h"
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/i18n.h"
#include "net/bambuddy_client.h"
#include "png.h"

#include <ArduinoJson.h>   // build synthetic /status payloads for the fixtures

#include <HTTPClient.h>   // raw request for the Cloudflare-Access diagnostic
#include <esp_heap_caps.h>   // free the camera-JPEG buffer

static const int W = ::display::WIDTH;
static const int H = ::display::HEIGHT;

static lv_color_t s_buf[480 * 272];
static lv_color_t s_fb[480 * 272];

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x)
            s_fb[y * W + x] = *px++;
    lv_disp_flush_ready(drv);
}

static void dump_png(const std::string& dir, const char* name) {
    static std::vector<uint8_t> rgb;
    rgb.resize((size_t)W * H * 3);
    for (int i = 0; i < W * H; ++i) {
        uint16_t c = s_fb[i].full;
        uint8_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
        rgb[i * 3 + 0] = (r5 << 3) | (r5 >> 2);
        rgb[i * 3 + 1] = (g6 << 2) | (g6 >> 4);
        rgb[i * 3 + 2] = (b5 << 3) | (b5 >> 2);
    }
    std::string path = dir + "/" + name + ".png";
    simpng::write_rgb(path.c_str(), rgb.data(), W, H);
    fprintf(stderr, "[sim] wrote %s\n", path.c_str());
}

static void pump(int frames) {
    for (int i = 0; i < frames; ++i) {
        lv_tick_inc(8);           // LV_TICK_CUSTOM is off in the sim
        ui::g_ui.refresh();
        lv_timer_handler();
        delay(8);
    }
}

// Bambuddy URL the Settings screen reads (owned by main.cpp on device; the
// shim defines it). The demo points it at a placeholder host.
extern String g_cfg_bambuddy_url;

// Push a synthetic /status payload through the REAL handler (the same one the
// REST path uses). apply_status_payload creates the printer on first
// sight, so no network / fetch is needed.
static void demo_status(int id, const char* json) {
    JsonDocument d;
    if (deserializeJson(d, json)) return;   // ignore malformed demo JSON
    bambuddy::g_client.apply_status_payload(id, d.as<JsonVariantConst>());
}

// A deterministic demo dataset (no network, no secrets) so the README
// screenshots show the device IN ACTION — a print running, a populated queue, a
// drying multi-slot AMS, real-looking history — instead of whatever the owner's
// printer happens to be doing. Renders the six README screens, driving the real
// apply_*_payload handlers. The shim's getLocalTime()/millis() are pinned so the
// time-derived UI (ETA, uptime) renders byte-reproducibly for visual regression.
static void render_fixtures(const std::string& out) {
    auto& C = bambuddy::g_client;
    JsonDocument d;

    // Boot / "loading" splash — rendered before any data is injected, so the
    // overlay (built visible in ui::Manager::begin) is still up. Draw it with
    // lv_timer_handler() directly, NOT pump(): pump() calls ui.refresh(), whose
    // hide logic would dismiss the splash the moment it sees no printers.
    lv_timer_handler(); lv_timer_handler();
    dump_png(out, "boot");

    // A small farm: #1 (focused) mid-print with a chained AMS — first unit
    // drying; #2 also printing; #3 idle.
    demo_status(1,
        "{\"name\":\"Workshop X1C\",\"model\":\"X1C\",\"state\":\"printing\","
        "\"progress\":62,\"remaining_time\":4320,\"layer_num\":164,\"total_layers\":267,"
        "\"subtask_name\":\"benchy_v2_pla.3mf\","
        "\"temperatures\":{\"nozzle\":220,\"bed\":60,\"chamber\":38},"
        "\"speed_level\":2,\"chamber_light\":true,"
        "\"cooling_fan_speed\":100,\"big_fan1_speed\":40,\"big_fan2_speed\":0,\"hms_errors\":[],"
        "\"ams_exists\":true,\"ams\":["
          "{\"id\":0,\"is_ams_ht\":false,\"humidity\":24,\"temp\":35,\"dry_time\":92,\"tray\":["
            "{\"id\":0,\"tray_color\":\"F5F5F5FF\",\"tray_type\":\"PLA\",\"remain\":100},"
            "{\"id\":1,\"tray_color\":\"101010FF\",\"tray_type\":\"PLA\",\"remain\":63},"
            "{\"id\":2,\"tray_color\":\"00000000\",\"tray_type\":\"PETG\",\"remain\":88},"
            "{\"id\":3,\"tray_color\":\"FF6A00FF\",\"tray_type\":\"TPU\",\"remain\":41}]},"
          "{\"id\":1,\"is_ams_ht\":true,\"humidity\":12,\"temp\":52,\"dry_time\":0,\"tray\":["
            "{\"id\":0,\"tray_color\":\"1E5FBEFF\",\"tray_type\":\"PA-CF\",\"remain\":77}]}]}");
    demo_status(2,
        "{\"name\":\"Garage P1S\",\"model\":\"P1S\",\"state\":\"printing\","
        "\"progress\":34,\"remaining_time\":7980,"
        "\"temperatures\":{\"nozzle\":245,\"bed\":80,\"chamber\":42}}");
    demo_status(3,
        "{\"name\":\"Office A1\",\"model\":\"A1\",\"state\":\"idle\","
        "\"temperatures\":{\"nozzle\":28,\"bed\":24}}");

    // Pending print queue (targets resolve to the printers above; -1 = unassigned).
    if (!deserializeJson(d,
        "[{\"status\":\"pending\",\"archive_name\":\"phone_stand_v3.3mf\",\"printer_id\":1,\"position\":1},"
        "{\"status\":\"pending\",\"archive_name\":\"calibration_cube.3mf\",\"printer_id\":2,\"position\":2},"
        "{\"status\":\"pending\",\"archive_name\":\"gridfinity_2x2.3mf\",\"printer_id\":-1,\"position\":3}]"))
        C.apply_queue_payload(d.as<JsonVariantConst>());

    // History: lifetime stats + recent prints.
    if (!deserializeJson(d,
        "{\"total_prints\":128,\"successful_prints\":120,\"failed_prints\":8,"
        "\"total_print_time_hours\":412.5,\"total_filament_grams\":3240}"))
        C.apply_stats_payload(d.as<JsonVariantConst>());
    if (!deserializeJson(d,
        "[{\"print_name\":\"benchy_v2_pla.3mf\",\"status\":\"success\",\"actual_time_seconds\":1080,\"filament_used_grams\":12},"
        "{\"print_name\":\"phone_stand_v3.3mf\",\"status\":\"success\",\"actual_time_seconds\":3840,\"filament_used_grams\":41},"
        "{\"print_name\":\"vase_spiral.3mf\",\"status\":\"failed\",\"actual_time_seconds\":600,\"filament_used_grams\":7},"
        "{\"print_name\":\"bracket_v2.3mf\",\"status\":\"success\",\"actual_time_seconds\":5400,\"filament_used_grams\":58}]"))
        C.apply_recent_payload(d.as<JsonVariantConst>());

    // Bambuddy server info + a configured URL (Settings shows both).
    if (!deserializeJson(d, "{\"app\":{\"version\":\"0.2.4\"},\"system\":{\"uptime_formatted\":\"5d 12h 18m\"}}"))
        C.apply_system_info_payload(d.as<JsonVariantConst>());
    g_cfg_bambuddy_url = "https://bambuddy.local";

    ui::g_ui.set_selected_printer(1);
    ui::screens::header_set_online(true, 24);

    // Inject a demo camera frame so the Live thumbnail renders a real picture —
    // deterministic fixtures have no network to fetch one. Decoded through the
    // same TJpg_Decoder shim the device uses, from the committed demo asset, so
    // the rendered "camera" tile is faithful to a real printer view.
#ifdef BAMBOARD_SIM_DEMO_DIR
    {
        FILE* cf = fopen(BAMBOARD_SIM_DEMO_DIR "/camera.jpg", "rb");
        if (cf) {
            fseek(cf, 0, SEEK_END);
            long n = ftell(cf);
            fseek(cf, 0, SEEK_SET);
            if (n > 0) {
                std::vector<uint8_t> jpg((size_t)n);
                if (fread(jpg.data(), 1, (size_t)n, cf) == (size_t)n)
                    ui::screens::camera_decode_frame(jpg.data(), jpg.size());
            }
            fclose(cf);
        }
    }
#endif

    // The six README screens, named to match docs/screenshots / the Pages deploy.
    ui::g_ui.go_to(ui::Screen::Dashboard); pump(30); dump_png(out, "live");
    ui::g_ui.go_to(ui::Screen::Ams);       pump(30); dump_png(out, "ams");
    ui::g_ui.go_to(ui::Screen::Printers);  pump(30); dump_png(out, "printers");
    ui::g_ui.go_to(ui::Screen::Queue);     pump(30); dump_png(out, "queue");
    ui::g_ui.go_to(ui::Screen::History);   pump(30); dump_png(out, "history");
    ui::g_ui.go_to(ui::Screen::Settings);  pump(30); dump_png(out, "settings");

    // Ambient idle clock. Drive the redraw directly — NOT pump(), which calls
    // Manager::refresh(): its gating would re-hide the overlay here because the
    // demo farm is "printing" (i.e. not quiet). The pinned shim clock makes it
    // render deterministically.
    ui::screens::ambient_show();
    lv_tick_inc(8); lv_timer_handler();
    lv_tick_inc(8); lv_timer_handler();
    dump_png(out, "ambient");
    ui::screens::ambient_hide();

    // Temperature graph — pre-fill the chart with a demo warm-up ramp, then
    // open + dump (same direct-redraw trick; refresh() isn't involved here).
    for (int k = 0; k < 90; ++k) {
        float t = (float)k / 89.0f;
        ui::screens::temp_graph_push(1, 25.f + t * 195.f, 22.f + t * 38.f, 24.f + t * 14.f);
    }
    ui::screens::temp_graph_open();
    lv_tick_inc(8); lv_timer_handler();
    lv_tick_inc(8); lv_timer_handler();
    dump_png(out, "tempgraph");
    ui::screens::temp_graph_close();

    // Print-complete notification (full-screen). Same direct-redraw trick.
    ui::screens::print_done_show("Workshop X1C", true);
    lv_tick_inc(8); lv_timer_handler();
    lv_tick_inc(8); lv_timer_handler();
    dump_png(out, "printdone");
    ui::screens::print_done_hide();

    // HMS full-screen flash — the most important safety signal, so it gets a
    // committed baseline too (title + dismiss hint are localized). A healthy
    // demo farm never raises it, so force it open with a sample code. The pulse
    // animation is phase-deterministic given the fixed tick steps below.
    ui::g_ui.go_to(ui::Screen::Dashboard); pump(10);
    ui::screens::hms_flash_show("HMS_0300_0100");
    lv_tick_inc(8); lv_timer_handler();
    lv_tick_inc(8); lv_timer_handler();
    dump_png(out, "hmsflash");
    ui::screens::hms_flash_hide();
}

int main(int argc, char** argv) {
    std::string out = getenv("SIM_OUT") ? getenv("SIM_OUT") : "sim_out";
    mkdir(out.c_str(), 0755);

    lv_init();
    static lv_disp_draw_buf_t dbuf;
    lv_disp_draw_buf_init(&dbuf, s_buf, nullptr, W * H);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = W; drv.ver_res = H;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &dbuf;
    drv.full_refresh = 1;
    lv_disp_drv_register(&drv);

    // Language
    const char* lang = getenv("BAMBOARD_LANG");
    int li = lang ? ::i18n::lang_from_code(lang) : 0;
    ::i18n::set_language(li >= 0 ? (uint8_t)li : 0);

    // Deterministic fixture renders for visual-regression — no network, no
    // secrets. Skips the live fetch entirely so it runs on every PR.
    if (getenv("SIM_FIXTURES_ONLY")) {
        ui::g_ui.begin();
        render_fixtures(out);
        return 0;
    }

    // Real Bambuddy data (no fabricated values).
    const char* url = getenv("BAMBUDDY_URL");
    const char* key = getenv("BAMBUDDY_API_KEY");
    const char* cf_id  = getenv("CF_ACCESS_CLIENT_ID");
    const char* cf_sec = getenv("CF_ACCESS_CLIENT_SECRET");
    if (url && *url) {
        String surl(url), skey(key ? key : "");
        String scf_id(cf_id ? cf_id : ""), scf_sec(cf_sec ? cf_sec : "");
        // Pass the CF token through the firmware client so begin_request() emits
        // the CF-Access headers itself — this exercises the real firmware path
        // instead of relying on the libcurl shim's env injection (which now only
        // backstops the raw diagnostic probes below).
        bambuddy::g_client.begin(surl, skey, scf_id, scf_sec);
        fprintf(stderr, "[sim] fetching from %s\n", url);
        // Raw diagnostic: who sends the 403 — Cloudflare Access or the origin?
        // Print the response body so the page identifies itself.
        {
            HTTPClient raw;
            raw.begin(String(url) + "/health");
            int rc = raw.GET();
            String body = raw.getString();
            fprintf(stderr, "[sim] RAW /health: code=%d len=%d\n[sim] body: %.250s\n",
                    rc, (int)body.length(), body.c_str());
            raw.end();
        }
        uint32_t lat = 0;
        bool h = bambuddy::g_client.ping_health(&lat);
        fprintf(stderr, "[sim] /health: %s (%u ms)\n", h ? "ok" : "FAIL", (unsigned)lat);
        bool ok = bambuddy::g_client.fetch_printers();
        fprintf(stderr, "[sim] fetch_printers: %s\n", ok ? "ok" : "FAILED");
        if (!ok) {
            String e;
            bambuddy::g_client.last_error(e);
            fprintf(stderr, "[sim] last_error: %s\n", e.c_str());
        }
        bambuddy::Printer ps[8]; uint8_t n = 0;
        bambuddy::g_client.snapshot_printers(ps, n);
        fprintf(stderr, "[sim] %u printer(s)\n", (unsigned)n);
        if (n > 0) {
            // Dump the real /status JSON to see the hms_errors structure (the
            // device showed a false HMS error — investigate the real shape).
            HTTPClient raw;
            raw.begin(String(url) + "/api/v1/printers/" + ps[0].id + "/status");
            raw.addHeader("X-API-Key", skey);
            raw.GET();
            String b = raw.getString();
            fprintf(stderr, "[sim] RAW /status len=%d:\n%.2000s\n", (int)b.length(), b.c_str());
            raw.end();
        }
        for (uint8_t i = 0; i < n; ++i) bambuddy::g_client.fetch_printer_status(ps[i].id);
        bambuddy::g_client.fetch_statistics();
        bambuddy::g_client.fetch_recent_archives(bambuddy::MAX_RECENT_ARCHIVES);
        bambuddy::g_client.fetch_queue();
        bambuddy::g_client.fetch_system_info();
        if (n > 0) ui::g_ui.set_selected_printer(ps[0].id);
    } else {
        fprintf(stderr, "[sim] no BAMBUDDY_URL — rendering empty states\n");
    }

    ui::g_ui.begin();

    // Camera: pull one real snapshot from the printer (via Bambuddy) and decode
    // it through the now-real TJpg_Decoder shim, so the Live thumbnail and the
    // full-screen viewer render the actual frame instead of empty chrome. The
    // overlay built by begin() above owns the decode buffer.
    {
        bambuddy::Printer cps[8]; uint8_t cn = 0;
        bambuddy::g_client.snapshot_printers(cps, cn);
        if (cn > 0) {
            uint8_t* jpeg = nullptr; size_t jlen = 0;
            if (bambuddy::g_client.fetch_camera_jpeg(cps[0].id, &jpeg, &jlen)) {
                fprintf(stderr, "[sim] camera: %u bytes -> decode\n", (unsigned)jlen);
                ui::screens::camera_decode_frame(jpeg, jlen);
                heap_caps_free(jpeg);
            } else {
                String e; bambuddy::g_client.last_error(e);
                fprintf(stderr, "[sim] camera: no frame (%s)\n", e.c_str());
            }
        }
    }

    struct { ui::Screen s; const char* name; } screens[] = {
        {ui::Screen::Dashboard, "dashboard"},
        {ui::Screen::Ams,       "ams"},
        {ui::Screen::Printers,  "printers"},
        {ui::Screen::Queue,     "queue"},
        {ui::Screen::History,   "history"},
        {ui::Screen::Settings,  "settings"},
    };
    for (auto& sc : screens) {
        ui::g_ui.go_to(sc.s);
        pump(30);
        // Dismiss the global HMS full-screen flash before capturing so we see
        // the actual screen underneath (a real printer with an active HMS error
        // would otherwise cover every screenshot during the flash's 5 s window).
        ui::screens::hms_flash_hide();
        lv_tick_inc(8);
        lv_timer_handler();
        dump_png(out, sc.name);
    }
    // Also capture the HMS full-screen flash. A healthy printer never triggers
    // it (apply_status_payload forces hms="ok" unless the printer is faulted),
    // so force it open with a sample code — otherwise this PNG would just
    // re-capture the dashboard and the overlay would never be validated.
    ui::g_ui.go_to(ui::Screen::Dashboard);
    pump(10);
    ui::screens::hms_flash_show("HMS_0300_0100");
    lv_tick_inc(8);
    lv_timer_handler();
    dump_png(out, "hms_overlay");

    // Full-screen camera viewer — shows the decoded frame if the fetch above
    // succeeded, otherwise its empty chrome.
    ui::g_ui.go_to(ui::Screen::Dashboard);
    ui::screens::camera_overlay_open();
    pump(10);
    dump_png(out, "camera");
    return 0;
}
