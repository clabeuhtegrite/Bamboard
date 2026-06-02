// Host unit tests for Bamboard's pure logic — no Arduino HW, no network, no
// secrets. Built + run in CI (.github/workflows/tests.yml) on every push / PR.
// Covers the bits most worth locking down against regressions: version
// comparison, the captive-portal host validation, and i18n table integrity.

#include <cstdio>
#include <cstring>

#include "net/semver.h"
#include "net/host_valid.h"
#include "ui/i18n.h"
#include "ui/dry_default.h"

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

int main() {
    test_semver();
    test_host_valid();
    test_i18n();
    test_dry_default();
    fprintf(stderr, "\n%d/%d checks passed\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
