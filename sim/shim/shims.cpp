// Implementations for the host shims + the symbols the firmware UI expects
// from main.cpp (which the sim does not compile).
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "../../firmware/src/hw/display.h"
#include "../../firmware/src/net/ca_bundle.h"   // CA-bundle symbol (stub below)
#include "../../firmware/src/ui/control.h"      // ui::ctrl::enqueue stub (below)

#ifdef SIM_HAVE_CURL
#include <curl/curl.h>
#endif

// Real JPEG decoder backing the TJpg_Decoder shim's camera path. STBI_ONLY_JPEG
// / STBI_NO_STDIO are set by <TJpg_Decoder.h> (included above); this TU provides
// the single implementation.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// ---- timing ----------------------------------------------------------------
uint32_t millis() {
    // Pinned in demo-fixture mode so time-derived UI (Settings uptime) renders
    // byte-reproducibly for the visual-regression baselines. Real clock otherwise.
    static const bool demo = getenv("SIM_FIXTURES_ONLY") != nullptr;
    if (demo) return 70980000u;   // -> uptime "19h 43m 00s"
    static auto t0 = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
}
void delay(uint32_t ms) {
    struct timespec ts { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000L) };
    nanosleep(&ts, nullptr);
}

// ---- globals expected by the firmware --------------------------------------
EspClass    ESP;
WiFiClass   WiFi;
TJpg_Decoder TJpgDec;

namespace hw { Display g_display; }

// Config globals normally owned by main.cpp; the harness sets the URL/key.
String  g_cfg_bambuddy_url;
String  g_cfg_api_key;
uint8_t g_cfg_brightness_level = 3;
void save_brightness_level(uint8_t level) { g_cfg_brightness_level = level; }
void factory_reset() {}
void reconfigure_wifi() {}
const char* g_boot_reason = "power-on";  // main.cpp global; the sim shows a stable value
// Control actions are no-ops in the sim — fixtures never tap the buttons and
// there is no net task to marshal them to.
namespace ui::ctrl {
void enqueue(Op, int, uint8_t, uint8_t, uint16_t) {}
}

// ---- HTTPClient over libcurl ----------------------------------------------
bool HTTPClient::begin(const String& url) { url_ = url.std_str(); hdrs_.clear(); size_ = -1; code_ = 0; return true; }
// Device https path is begin(secureClient, url); the sim ignores the client and
// lets libcurl handle TLS over the system trust store, so this just forwards.
bool HTTPClient::begin(WiFiClientSecure&, const String& url) { return begin(url); }
void HTTPClient::addHeader(const String& k, const String& v) { hdrs_.emplace_back(k.std_str(), v.std_str()); }
int  HTTPClient::getSize() { return (int)size_; }
bool HTTPClient::connected() { return stream_.connected(); }
Stream& HTTPClient::getStream() { return stream_; }
WiFiClient* HTTPClient::getStreamPtr() { return &stream_; }
String HTTPClient::getString() { std::string s; while (stream_.available()) s.push_back((char)stream_.read()); return String(s); }
String HTTPClient::errorToString(int code) { return String("curl error ") + code; }
void HTTPClient::end() {}

#ifdef SIM_HAVE_CURL
static size_t sim_curl_write(char* p, size_t sz, size_t nm, void* ud) {
    ((std::string*)ud)->append(p, sz * nm);
    return sz * nm;
}
int HTTPClient::perform(const char* method, const std::string& body) {
    static bool inited = false;
    if (!inited) { curl_global_init(CURL_GLOBAL_DEFAULT); inited = true; }
    CURL* c = curl_easy_init();
    if (!c) return -1;
    std::string resp;
    curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, sim_curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* hl = nullptr;
    bool have_cf = false;
    for (auto& h : hdrs_) {
        hl = curl_slist_append(hl, (h.first + ": " + h.second).c_str());
        if (h.first == "CF-Access-Client-Id") have_cf = true;
    }
    // Cloudflare Access service token from the environment — only as a backstop
    // for requests that didn't already carry it (the raw diagnostic probes in
    // main.cpp). The firmware client now sets these headers itself via
    // begin_request(), so this prevents sending them twice on a client request.
    if (!have_cf) {
        const char* cf_id = getenv("CF_ACCESS_CLIENT_ID");
        const char* cf_sec = getenv("CF_ACCESS_CLIENT_SECRET");
        if (cf_id && *cf_id)  hl = curl_slist_append(hl, (std::string("CF-Access-Client-Id: ") + cf_id).c_str());
        if (cf_sec && *cf_sec) hl = curl_slist_append(hl, (std::string("CF-Access-Client-Secret: ") + cf_sec).c_str());
    }
    if (hl) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hl);

    if (!strcmp(method, "POST")) {
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)body.size());
    }
    CURLcode rc = curl_easy_perform(c);
    long http_code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    if (hl) curl_slist_free_all(hl);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        // Surface the transport-level reason (timeout / connect refused / TLS …)
        // so a failed fetch in CI is diagnosable instead of a bare "-1".
        fprintf(stderr, "[sim] curl %s %s -> %s\n", method, url_.c_str(),
                curl_easy_strerror(rc));
        code_ = -1;
        return -1;
    }
    stream_.sim_reset(resp);
    size_ = (long)resp.size();
    code_ = (int)http_code;
    return code_;
}
#else
int HTTPClient::perform(const char*, const std::string&) { code_ = -1; return -1; }
#endif

int HTTPClient::GET()                         { return perform("GET", ""); }
int HTTPClient::POST(const String& body)      { return perform("POST", body.std_str()); }
int HTTPClient::POST(uint8_t* body, size_t n) { return perform("POST", std::string((char*)body, body ? n : 0)); }

// CA-bundle linker-symbol stub. On device the bundle is embedded via
// board_build.embed_files; the sim can't embed a file and validates TLS over
// libcurl's system trust store, so a 1-byte placeholder satisfies the linker
// (the firmware hands it to the no-op WiFiClientSecure::setCACertBundle).
const uint8_t ca_bundle_start[1] = {0};
