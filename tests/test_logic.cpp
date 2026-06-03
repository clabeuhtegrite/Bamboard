// Host unit tests for Bamboard's pure logic — no Arduino HW, no network, no
// secrets. Built + run in CI (.github/workflows/tests.yml) on every push / PR.
// Covers the bits most worth locking down against regressions: version
// comparison, the captive-portal host validation, and i18n table integrity.

#include <cmath>
#include <cstdio>
#include <cstring>

#include <ArduinoJson.h>

#include "config.h"               // bambuddy::MAX_PRINTERS / MAX_QUEUE_ITEMS / …
#include "net/bambuddy_client.h"
#include "net/host_valid.h"
#include "net/semver.h"
#include "ui/dry_default.h"
#include "ui/i18n.h"
#include "ui/units.h"

static int g_total = 0, g_fail = 0;
#define CHECK(cond) do { ++g_total; if (!(cond)) { ++g_fail; \
    fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void test_semver() {
    using ota::semver_cmp;
    CHECK(semver_cmp("1.2.3", "1.2.2") > 0);
    CHECK(semver_cmp("1.2.2", "1.2.3") < 0);
    CHECK(semver_cmp("1.2.3", "1.2.3") == 0);
    CHECK(semver_cmp("0.18.0", "0.17.3") > 0);
    CHECK(semver_cmp("v0.18.0", "0.18.0") == 0);     // leading v ignored
    CHECK(semver_cmp("1.0.0-rc1", "1.0.0") == 0);    // pre-release suffix ignored
    CHECK(semver_cmp("2.0.0", "1.9.9") > 0);
    CHECK(semver_cmp("1.10.0", "1.9.0") > 0);        // numeric, not lexical
    CHECK(semver_cmp("", "0.0.0") == 0);             // empty -> 0.0.0
    // A pathological huge component must not flip the result via overflow.
    CHECK(semver_cmp("0.0.99999999999999999999", "0.0.1") > 0);
}

static void test_host_valid() {
    using namespace netcfg;
    // private IPv4
    CHECK(is_private_ipv4("192.168.1.10"));
    CHECK(is_private_ipv4("10.0.0.1"));
    CHECK(is_private_ipv4("172.16.0.1"));
    CHECK(is_private_ipv4("172.31.255.255"));
    CHECK(is_private_ipv4("127.0.0.1"));
    CHECK(!is_private_ipv4("172.15.0.1"));   // just below the /12
    CHECK(!is_private_ipv4("172.32.0.1"));   // just above the /12
    CHECK(!is_private_ipv4("8.8.8.8"));
    CHECK(!is_private_ipv4("256.0.0.1"));    // octet out of range
    CHECK(!is_private_ipv4("192.168.1"));    // too few octets
    // mDNS
    CHECK(is_mdns_local("bambuddy.local"));
    CHECK(is_mdns_local("Bambuddy.LOCAL"));  // case-insensitive
    CHECK(!is_mdns_local(".local"));         // bare suffix
    CHECK(!is_mdns_local("bambuddy.lan"));
    // hostname
    CHECK(is_hostname("bambuddy.example.com"));
    CHECK(is_hostname("host-1.lan"));
    CHECK(!is_hostname("-bad.example.com"));  // label starts with '-'
    CHECK(!is_hostname("bad-.example.com"));  // label ends with '-'
    CHECK(!is_hostname("bad_host.com"));      // underscore not allowed
    CHECK(!is_hostname(""));
    // scheme gating
    CHECK(host_valid_for_scheme("192.168.1.10", false));   // http + private IP OK
    CHECK(host_valid_for_scheme("printer.local", false));  // http + .local OK
    CHECK(!host_valid_for_scheme("example.com", false));   // http + public domain rejected
    CHECK(!host_valid_for_scheme("8.8.8.8", false));       // http + public IP rejected
    CHECK(host_valid_for_scheme("example.com", true));     // https + domain OK
    CHECK(host_valid_for_scheme("8.8.8.8", true));         // https + any IP OK
    // host:port split
    String h, p;
    split_host_port("192.168.1.10:8080", h, p);
    CHECK(h == "192.168.1.10" && p == "8080");
    split_host_port("printer.local", h, p);
    CHECK(h == "printer.local" && p == "");
    CHECK(digits_only("8080"));
    CHECK(!digits_only(""));
    CHECK(!digits_only("80a"));
}

// Collect the sequence of printf conversion specifiers in a format string
// ("Brightness %u / 5" -> "u"). "%%" is a literal percent, skipped.
static void fmt_specs(const char* s, char* out, size_t cap) {
    size_t n = 0;
    for (const char* p = s; *p && n + 1 < cap; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == '%' || !*p) continue;            // "%%" or trailing '%'
        while (*p && !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')))
            ++p;                                   // skip flags / width / precision
        if (*p) out[n++] = *p;
    }
    out[n] = '\0';
}

static void test_i18n() {
    const int L = (int)i18n::Lang::COUNT;
    CHECK(L == 5);
    for (int s = 0; s < (int)i18n::Str::_COUNT; ++s) {
        i18n::set_language((uint8_t)i18n::Lang::EN);
        char en_specs[16];
        fmt_specs(i18n::tr((i18n::Str)s), en_specs, sizeof(en_specs));
        for (int l = 0; l < L; ++l) {
            i18n::set_language((uint8_t)l);
            const char* t = i18n::tr((i18n::Str)s);
            CHECK(t != nullptr && t[0] != '\0');       // no empty cell in any language
            char specs[16];
            fmt_specs(t, specs, sizeof(specs));
            CHECK(strcmp(specs, en_specs) == 0);       // identical %-specs across languages
        }
    }
    // two-letter language code round-trip
    for (int l = 0; l < L; ++l)
        CHECK(i18n::lang_from_code(i18n::lang_code((uint8_t)l)) == l);
    CHECK(i18n::lang_from_code("xx") < 0);             // unknown code
}

static void test_dry_default() {
    using ui::dry_default_for;
    // Known types map to their fallback setpoint.
    CHECK(dry_default_for("PLA").temp_c == 55);
    CHECK(dry_default_for("TPU").temp_c == 50 && dry_default_for("TPU").hours == 12);
    CHECK(dry_default_for("ABS").temp_c == 80);
    CHECK(dry_default_for("PETG").temp_c == 65);
    // Prefix ordering: the more specific entry must win.
    CHECK(dry_default_for("PPS").temp_c == 90);   // PPS listed before PP
    CHECK(dry_default_for("PP").temp_c  == 65);   // bare PP
    // "PA" is the nylon catch-all (PA, PAHT, PA-CF…).
    CHECK(dry_default_for("PA-CF").temp_c == 80 && dry_default_for("PA-CF").hours == 12);
    CHECK(dry_default_for("PAHT").temp_c == 80);
    // Unknown / empty / null → no recommendation.
    CHECK(dry_default_for("WOOD").prefix == nullptr);
    CHECK(dry_default_for("").prefix     == nullptr);
    CHECK(dry_default_for(nullptr).prefix == nullptr);
}

// ---------------------------------------------------------------------------
// Bambuddy REST client — JSON payload parsers (apply_*_payload).
//
// These run the REAL client against hand-built fixtures of Bambuddy's wire
// shapes, locking down the parse logic (state mapping, dual temperature naming,
// HMS surfacing/suppression, AMS tray colour/alpha/drying clamp, field
// fallbacks) without any network. The apply_* methods were split out of the
// fetch_* methods precisely so they could be host-tested like this.
// ---------------------------------------------------------------------------

static bool feq(float a, float b) { return std::fabs(a - b) < 0.05f; }

static void test_status_parse() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    const char* js =
        "{"
        "\"state\":\"printing\",\"progress\":42.7,\"remaining_time\":1234,"
        "\"layer_num\":10,\"total_layers\":200,"
        "\"temperatures\":{\"nozzle\":210.5,\"bed\":60.0,\"chamber\":35.0},"
        "\"subtask_name\":\"benchy.gcode\",\"speed_level\":3,"
        "\"chamber_light\":true,\"cooling_fan_speed\":80,"
        "\"big_fan1_speed\":50,\"big_fan2_speed\":0,"
        "\"awaiting_plate_clear\":false,\"hms_errors\":[],"
        "\"ams_exists\":true,"
        "\"ams\":[{\"id\":0,\"is_ams_ht\":false,\"humidity\":25,\"temp\":28.5,"
        "\"dry_time\":0,\"tray\":["
        "{\"id\":0,\"tray_color\":\"FF6A00FF\",\"tray_type\":\"PLA\","
        "\"remain\":75,\"drying_temp\":55,\"drying_time\":8},"
        "{\"id\":1,\"tray_color\":\"0000007F\",\"tray_type\":\"PETG\",\"remain\":50}"
        "]}]"
        "}";
    JsonDocument doc;
    CHECK(!deserializeJson(doc, js));
    CHECK(c.apply_status_payload(7, doc.as<JsonVariantConst>()));

    bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
    uint8_t n = 0;
    c.snapshot_printers(ps, n);
    CHECK(n == 1);
    const bambuddy::Printer& p = ps[0];
    CHECK(p.id == 7);
    CHECK(p.state == bambuddy::PrinterState::Printing);
    CHECK(p.progress == 42);                 // float 42.7 truncated to a uint8
    CHECK(p.remaining_s == 1234);
    CHECK(p.current_layer == 10 && p.total_layers == 200);
    CHECK(feq(p.temps.nozzle, 210.5f) && feq(p.temps.bed, 60.0f) &&
          feq(p.temps.chamber, 35.0f));
    CHECK(p.speed_level == 3);
    CHECK(p.chamber_light);
    CHECK(p.fan_cooling == 80 && p.fan_aux == 50 && p.fan_chamber == 0);
    CHECK(p.filename == "benchy.gcode");
    CHECK(p.hms == "ok");                     // empty hms_errors → ok
    CHECK(p.ams_exists && p.ams_count == 1);
    const bambuddy::AmsUnit& u = p.ams[0];
    CHECK(u.humidity == 25 && feq(u.temp, 28.5f) && u.slot_count == 2);
    // Slot 0: solid orange, opaque (alpha FF) → RGB kept, not translucent.
    CHECK(u.slots[0].color_rgb == 0xFF6A00u && !u.slots[0].translucent);
    CHECK(strcmp(u.slots[0].type, "PLA") == 0 && u.slots[0].remain == 75);
    CHECK(u.slots[0].dry_temp_c == 55 && u.slots[0].dry_time_h == 8);
    // Slot 1: alpha 0x7F < 0x80 → translucent; RGB 0 but still "present".
    CHECK(u.slots[1].translucent && u.slots[1].color_rgb == 0u);
    CHECK(strcmp(u.slots[1].type, "PETG") == 0 && u.slots[1].present);
}

static void test_status_temp_alt_naming() {
    // Older Bambu firmware reports nozzle_temper / bed_temper / chamber_temper.
    bambuddy::Client c;
    c.begin("http://x", "k");
    const char* js =
        "{\"state\":\"idle\",\"temperatures\":"
        "{\"nozzle_temper\":25.0,\"bed_temper\":22.0,\"chamber_temper\":21.0}}";
    JsonDocument doc;
    CHECK(!deserializeJson(doc, js));
    CHECK(c.apply_status_payload(1, doc.as<JsonVariantConst>()));
    bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
    uint8_t n = 0;
    c.snapshot_printers(ps, n);
    CHECK(n == 1 && feq(ps[0].temps.nozzle, 25.0f) &&
          feq(ps[0].temps.bed, 22.0f) && feq(ps[0].temps.chamber, 21.0f));
}

static void test_status_hms() {
    // Active fault state (paused/failed/error) surfaces the HMS code; a healthy
    // or finished state suppresses low-severity HMS noise → "ok".
    {
        bambuddy::Client c;
        c.begin("http://x", "k");
        JsonDocument doc;
        CHECK(!deserializeJson(doc,
            "{\"state\":\"paused\",\"hms_errors\":[{\"code\":\"0300_0100\"}]}"));
        c.apply_status_payload(1, doc.as<JsonVariantConst>());
        bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
        uint8_t n = 0;
        c.snapshot_printers(ps, n);
        CHECK(ps[0].hms == "0300_0100");
    }
    {
        bambuddy::Client c;
        c.begin("http://x", "k");
        JsonDocument doc;
        CHECK(!deserializeJson(doc,
            "{\"state\":\"finish\",\"hms_errors\":[{\"code\":\"0300_0100\"}]}"));
        c.apply_status_payload(1, doc.as<JsonVariantConst>());
        bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
        uint8_t n = 0;
        c.snapshot_printers(ps, n);
        CHECK(ps[0].hms == "ok");          // healthy printer → HMS hidden
    }
    {
        bambuddy::Client c;
        c.begin("http://x", "k");
        JsonDocument doc;
        CHECK(!deserializeJson(doc,
            "{\"state\":\"failed\",\"hms_errors\":"
            "[{\"code\":\"AAAA\"},{\"code\":\"BBBB\"}]}"));
        c.apply_status_payload(1, doc.as<JsonVariantConst>());
        bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
        uint8_t n = 0;
        c.snapshot_printers(ps, n);
        CHECK(ps[0].hms == "AAAA (+1 more)");
    }
}

static void test_stats_parse() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    JsonDocument doc;
    CHECK(!deserializeJson(doc,
        "{\"total_prints\":10,\"successful_prints\":8,\"failed_prints\":2,"
        "\"total_print_time_hours\":5.5,\"total_filament_grams\":123.4}"));
    CHECK(c.apply_stats_payload(doc.as<JsonVariantConst>()));
    bambuddy::Stats s = c.snapshot_stats();
    CHECK(s.total_prints == 10 && s.successful_prints == 8 && s.failed_prints == 2);
    CHECK(s.total_time_s == 19800);        // 5.5 h * 3600
    CHECK(feq(s.total_filament_g, 123.4f));
    CHECK(feq(s.success_rate, 80.0f));     // computed from the counts
}

static void test_queue_parse() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    JsonDocument doc;
    CHECK(!deserializeJson(doc,
        "[{\"id\":1,\"archive_name\":\"a.gcode\",\"status\":\"pending\","
        "\"printer_id\":3,\"position\":1},"
        "{\"id\":2,\"archive_name\":\"b.gcode\",\"status\":\"done\","
        "\"printer_id\":3,\"position\":2},"
        "{\"id\":3,\"archive_name\":\"\",\"status\":\"pending\","
        "\"printer_id\":-1,\"position\":3}]"));
    CHECK(c.apply_queue_payload(doc.as<JsonVariantConst>()));
    bambuddy::QueueItem q[bambuddy::MAX_QUEUE_ITEMS];
    uint8_t n = 0;
    c.snapshot_queue(q, n);
    CHECK(n == 2);                         // only "pending" rows are kept
    CHECK(q[0].id == 1 && q[0].name == "a.gcode" && q[0].printer_id == 3);
    CHECK(q[1].id == 3 && q[1].name == "(file)" && q[1].printer_id == -1);
    // A non-array payload is rejected, not crashed on.
    JsonDocument bad;
    CHECK(!deserializeJson(bad, "{}"));
    CHECK(!c.apply_queue_payload(bad.as<JsonVariantConst>()));
}

static void test_recent_parse() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    JsonDocument doc;
    CHECK(!deserializeJson(doc,
        "[{\"print_name\":\"x\",\"status\":\"success\","
        "\"actual_time_seconds\":3600,\"filament_used_grams\":12.0},"
        "{\"print_name\":\"y\",\"status\":\"failed\","
        "\"print_time_seconds\":1800,\"filament_used_grams\":5.0}]"));
    CHECK(c.apply_recent_payload(doc.as<JsonVariantConst>()));
    bambuddy::Archive a[bambuddy::MAX_RECENT_ARCHIVES];
    uint8_t n = 0;
    c.snapshot_recent(a, n);
    CHECK(n == 2);
    CHECK(a[0].name == "x" && a[0].status == "success" &&
          a[0].duration_s == 3600 && feq(a[0].filament_g, 12.0f));
    CHECK(a[1].duration_s == 1800);        // falls back to print_time_seconds
}

static void test_system_info_parse() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    JsonDocument doc;
    CHECK(!deserializeJson(doc,
        "{\"app\":{\"version\":\"1.2.3\"},"
        "\"system\":{\"uptime_formatted\":\"2d 3h\"}}"));
    CHECK(c.apply_system_info_payload(doc.as<JsonVariantConst>()));
    bambuddy::SystemInfo si = c.snapshot_system_info();
    CHECK(si.version == "1.2.3" && si.uptime == "2d 3h");
}

static void test_parse_resilience() {
    bambuddy::Client c;
    c.begin("http://x", "k");
    // A null status variant is rejected without touching the cache.
    JsonDocument nil;
    CHECK(!c.apply_status_payload(1, nil.as<JsonVariantConst>()));
    // A status carrying only the state still parses, with safe defaults elsewhere.
    JsonDocument doc;
    CHECK(!deserializeJson(doc, "{\"state\":\"idle\"}"));
    CHECK(c.apply_status_payload(2, doc.as<JsonVariantConst>()));
    bambuddy::Printer ps[bambuddy::MAX_PRINTERS];
    uint8_t n = 0;
    c.snapshot_printers(ps, n);
    CHECK(n == 1 && ps[0].state == bambuddy::PrinterState::Idle);
    CHECK(ps[0].progress == 0 && ps[0].ams_count == 0 && ps[0].hms == "ok");
}

static void test_units() {
    using ui::to_fahrenheit;
    CHECK(std::fabs(to_fahrenheit(0.0f)   -  32.0f) < 0.01f);
    CHECK(std::fabs(to_fahrenheit(100.0f) - 212.0f) < 0.01f);
    CHECK(std::fabs(to_fahrenheit(37.0f)  -  98.6f) < 0.01f);
    CHECK(std::fabs(to_fahrenheit(-40.0f) - (-40.0f)) < 0.01f);  // the °C/°F crossover
    char b[16];
    ui::format_clock_core(b, sizeof(b), 14, 32, true);  CHECK(strcmp(b, "14:32") == 0);
    ui::format_clock_core(b, sizeof(b),  9,  5, true);  CHECK(strcmp(b, "09:05") == 0);
    ui::format_clock_core(b, sizeof(b), 14, 32, false); CHECK(strcmp(b, "2:32 PM") == 0);
    ui::format_clock_core(b, sizeof(b),  0,  7, false); CHECK(strcmp(b, "12:07 AM") == 0);  // midnight
    ui::format_clock_core(b, sizeof(b), 12,  0, false); CHECK(strcmp(b, "12:00 PM") == 0);  // noon
    ui::format_clock_core(b, sizeof(b), 23, 59, false); CHECK(strcmp(b, "11:59 PM") == 0);
}

int main() {
    test_semver();
    test_host_valid();
    test_i18n();
    test_dry_default();
    test_units();
    test_status_parse();
    test_status_temp_alt_naming();
    test_status_hms();
    test_stats_parse();
    test_queue_parse();
    test_recent_parse();
    test_system_info_parse();
    test_parse_resilience();
    fprintf(stderr, "\n%d/%d checks passed\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
