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

#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "config.h"
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/i18n.h"
#include "net/bambuddy_client.h"
#include "net/bambuddy_ws.h"
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

// Push a synthetic /status payload through the REAL handler (the same one the
// WebSocket push path calls). apply_status_payload creates the printer on first
// sight, so no network / fetch is needed.
static void fixture_apply(int id, const char* json) {
    JsonDocument d;
    DeserializationError e = deserializeJson(d, json);
    if (e) { fprintf(stderr, "[sim] fixture parse error: %s\n", e.c_str()); return; }
    bambuddy::g_client.apply_status_payload(id, d.as<JsonVariantConst>());
}

// Deterministic synthetic states (no network, no secrets) so CI can validate —
// and pixel-diff against committed baselines — the printing / multi-AMS / HMS
// UI that the live data may never happen to show. getLocalTime() is pinned in
// the shim so the wall-clock ETA renders reproducibly.
static void render_fixtures(const std::string& out) {
    static const char* PRINTING =
        "{\"name\":\"Workshop X1C\",\"model\":\"X1C\",\"state\":\"printing\","
        "\"progress\":62,\"remaining_time\":4320,\"layer_num\":164,\"total_layers\":267,"
        "\"subtask_name\":\"benchy_v2_pla.3mf\","
        "\"temperatures\":{\"nozzle\":220,\"bed\":60,\"chamber\":38},"
        "\"speed_level\":2,\"chamber_light\":true,"
        "\"cooling_fan_speed\":100,\"big_fan1_speed\":40,\"big_fan2_speed\":0,"
        "\"hms_errors\":[]}";
    static const char* MULTI_AMS =
        "{\"name\":\"Workshop X1C\",\"model\":\"X1C\",\"state\":\"printing\","
        "\"progress\":62,\"remaining_time\":4320,"
        "\"temperatures\":{\"nozzle\":220,\"bed\":60,\"chamber\":38},"
        "\"ams_exists\":true,\"ams\":["
          "{\"id\":0,\"is_ams_ht\":false,\"humidity\":25,\"temp\":31,\"dry_time\":0,\"tray\":["
            "{\"id\":0,\"tray_color\":\"FFFFFFFF\",\"tray_type\":\"PLA\",\"remain\":100},"
            "{\"id\":1,\"tray_color\":\"000000FF\",\"tray_type\":\"PLA\",\"remain\":63},"
            "{\"id\":2,\"tray_color\":\"\",\"tray_type\":\"\",\"remain\":0},"
            "{\"id\":3,\"tray_color\":\"00000000\",\"tray_type\":\"PETG\",\"remain\":100}]},"
          "{\"id\":1,\"is_ams_ht\":true,\"humidity\":18,\"temp\":48,\"dry_time\":135,\"tray\":["
            "{\"id\":0,\"tray_color\":\"FF6A00FF\",\"tray_type\":\"ABS\",\"remain\":80}]}]}";
    static const char* HMS =
        "{\"name\":\"Workshop X1C\",\"model\":\"X1C\",\"state\":\"failed\",\"progress\":42,"
        "\"temperatures\":{\"nozzle\":218,\"bed\":60,\"chamber\":40},"
        "\"hms_errors\":[{\"code\":\"0300_0100\",\"severity\":\"fatal\"}]}";

    const int ID = 1;
    ui::g_ui.set_selected_printer(ID);

    fixture_apply(ID, PRINTING);                 // Live while printing
    ui::g_ui.go_to(ui::Screen::Dashboard); pump(30); dump_png(out, "fx_live_printing");

    ui::g_ui.go_to(ui::Screen::Printers);  pump(30); dump_png(out, "fx_printers");

    fixture_apply(ID, MULTI_AMS);                // unit 1: 4 slots (white / black / empty / clear PETG)
    ui::g_ui.go_to(ui::Screen::Ams);       pump(30); dump_png(out, "fx_ams_multi");
    ui::screens::ams_cycle_unit(+1);             // unit 2: AMS-HT with a live drying countdown
    pump(30); dump_png(out, "fx_ams_dry");

    fixture_apply(ID, HMS);                       // HMS full-screen flash
    ui::g_ui.go_to(ui::Screen::Dashboard); pump(30); dump_png(out, "fx_hms");
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
        // Pass the CF token through the firmware client/ws so begin_request()
        // and apply_url() emit the CF-Access headers themselves — this exercises
        // the real firmware path instead of relying on the libcurl shim's env
        // injection (which now only backstops the raw diagnostic probes below).
        bambuddy::g_client.begin(surl, skey, scf_id, scf_sec);
        bambuddy::g_ws.begin(surl, scf_id, scf_sec);
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
