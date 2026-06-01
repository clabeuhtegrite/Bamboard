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

    // Real Bambuddy data (no fabricated values).
    const char* url = getenv("BAMBUDDY_URL");
    const char* key = getenv("BAMBUDDY_API_KEY");
    if (url && *url) {
        String surl(url), skey(key ? key : "");
        bambuddy::g_client.begin(surl, skey);
        bambuddy::g_ws.begin(surl);
        fprintf(stderr, "[sim] fetching from %s\n", url);
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
        for (uint8_t i = 0; i < n; ++i) bambuddy::g_client.fetch_printer_status(ps[i].id);
        bambuddy::g_client.fetch_statistics();
        bambuddy::g_client.fetch_recent_archives(bambuddy::MAX_RECENT_ARCHIVES);
        if (n > 0) ui::g_ui.set_selected_printer(ps[0].id);
    } else {
        fprintf(stderr, "[sim] no BAMBUDDY_URL — rendering empty states\n");
    }

    ui::g_ui.begin();

    struct { ui::Screen s; const char* name; } screens[] = {
        {ui::Screen::Dashboard, "dashboard"},
        {ui::Screen::Ams,       "ams"},
        {ui::Screen::Printers,  "printers"},
        {ui::Screen::History,   "history"},
        {ui::Screen::Settings,  "settings"},
    };
    for (auto& sc : screens) {
        ui::g_ui.go_to(sc.s);
        pump(30);
        dump_png(out, sc.name);
    }
    return 0;
}
