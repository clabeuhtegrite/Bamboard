#include "github_ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../config.h"
#include "ca_bundle.h"    // embedded root-CA bundle (validate GitHub's TLS)

namespace ota {

// The download progress callback handed to us by the caller. httpUpdate /
// Update fire a C++ std::function during the flash write; we stash the plain
// function pointer here so that captureless lambda can reach it.
static void (*s_on_progress)(uint8_t) = nullptr;

// Version comparison (semver_cmp) now lives in net/semver.cpp so it can be
// unit-tested without this file's HTTP / Update dependencies; it's declared via
// github_ota.h, which includes semver.h.

// ---------------------------------------------------------------------------
// Manifest fetch
// ---------------------------------------------------------------------------

// Pulls manifest.json from the latest-release redirect and extracts the
// version + firmware-binary URL + expected MD5 + size. Returns false on any
// network / parse error.
static bool fetch_manifest(String& out_version, String& out_bin_url,
                           String& out_md5, size_t& out_size,
                           bool& server_replied) {
    server_replied = false;
    WiFiClientSecure client;
    // Validate GitHub's TLS chain against the embedded Mozilla root-CA bundle —
    // the same bundle (and the same setCACertBundle path) the device already
    // uses to reach Bambuddy over https. esp_crt_bundle is host-agnostic: it
    // accepts any chain rooting in a bundled CA, so it covers both github.com
    // and the asset CDN (objects.githubusercontent.com) the /latest/download
    // redirect lands on. This closes the manifest-in-flight MITM that plain
    // setInsecure() left open; the published MD5 (bound into Update.end()) and
    // the release-path pin on the binary URL (see check_and_update) are
    // defence-in-depth layered on top.
    client.setCACertBundle(ca_bundle_start);

    HTTPClient http;
    http.setConnectTimeout(::ota::CHECK_TIMEOUT_MS);
    http.setTimeout(::ota::CHECK_TIMEOUT_MS);
    // /releases/latest/download/<asset> 302s to the versioned asset URL.
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    if (!http.begin(client, ::ota::MANIFEST_URL)) {
        log_w("OTA: manifest begin() failed");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        log_w("OTA: manifest HTTP %d", code);
        http.end();
        return false;
    }
    // The server answered 200 — from here on, any failure is a bad manifest
    // (unparseable / missing fields), not a network problem.
    server_replied = true;

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        log_w("OTA: manifest parse error: %s", err.c_str());
        return false;
    }

    out_version = (const char*)(doc["version"] | "");
    out_bin_url = (const char*)(doc["url"] | "");
    out_md5     = (const char*)(doc["md5"] | "");
    out_size    = (size_t)(doc["size"] | 0);   // 0 if absent → size check skipped
    // md5 is optional for backwards compatibility with manifests published by
    // older release.yml workflows; an empty value just disables verification.
    return out_version.length() > 0 && out_bin_url.length() > 0;
}

// ---------------------------------------------------------------------------
// Check + apply
// ---------------------------------------------------------------------------

CheckResult check_and_update(void (*on_start)(), void (*on_progress)(uint8_t),
                             const char* skip_version) {
    if (WiFi.status() != WL_CONNECTED) {
        log_w("OTA: Wi-Fi down — skipping update check");
        return CheckResult::NoNetwork;
    }

    String latest, bin_url, expected_md5;
    size_t expected_size = 0;
    bool server_replied = false;
    if (!fetch_manifest(latest, bin_url, expected_md5, expected_size, server_replied)) {
        // Distinguish "couldn't reach the server" from "reached it but the
        // manifest was unparseable / missing fields" so the cause is diagnosable.
        return server_replied ? CheckResult::BadManifest : CheckResult::NoNetwork;
    }

    log_i("OTA: running %s, latest release %s", BAMBOARD_VERSION, latest.c_str());
    if (semver_cmp(latest.c_str(), BAMBOARD_VERSION) <= 0) {
        return CheckResult::UpToDate;
    }

    // Anti-brick: this exact version was rolled back as a bad build (it booted
    // but never confirmed healthy). Don't re-flash it — that would just loop —
    // until a newer release supersedes it. skip_version is the caller's
    // remembered bad version (null/empty = nothing flagged).
    if (skip_version && skip_version[0] && latest == skip_version) {
        log_w("OTA: latest %s is flagged bad (rolled back) — skipping", latest.c_str());
        return CheckResult::UpToDate;
    }

    // Provenance pin: the binary must live under this repo's own release-download
    // path (release.yml always publishes it there). A tampered manifest therefore
    // can't aim the flash at an arbitrary host — and since the fetch is already
    // validated against GitHub's TLS certs (CA bundle above), the download is both
    // authenticated to GitHub and bound to our releases. Checked before on_start()
    // so we never pop the overlay for an image we won't flash.
    if (!bin_url.startsWith(::ota::BIN_URL_PREFIX)) {
        log_e("OTA: bin url '%s' not under %s — refusing to flash",
              bin_url.c_str(), ::ota::BIN_URL_PREFIX);
        return CheckResult::Failed;
    }

    // Require a published MD5 before committing to a flash. The fetch is now
    // TLS-validated and the URL pinned to our releases (above), but the MD5 —
    // bound into Update.end() below — stays as an integrity belt-and-braces:
    // it catches a corrupted download and refuses a legacy/tampered manifest
    // that omitted it. No md5 ⇒ refuse. (release.yml always publishes one.)
    // Checked here, before on_start(), so we never pop the update overlay for
    // an image we won't flash.
    bool md5_valid = (expected_md5.length() == 32);
    for (size_t i = 0; md5_valid && i < 32; ++i) {
        char c = expected_md5[i];
        md5_valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');
    }
    if (!md5_valid) {
        log_e("OTA: newer release %s but md5 missing/invalid — refusing to flash",
              latest.c_str());
        return CheckResult::Failed;
    }

    // Pre-flight capacity check: refuse early (clean log, no overlay) when the
    // published size can't fit the inactive OTA slot, instead of starting a
    // download that Update.begin() would only reject mid-stream. Conservative —
    // gates solely when both numbers are known (size present, slot reported).
    uint32_t free_slot = ESP.getFreeSketchSpace();
    if (expected_size && free_slot && expected_size > free_slot) {
        log_e("OTA: image %u B exceeds free OTA slot %u B — refusing",
              (unsigned)expected_size, (unsigned)free_slot);
        return CheckResult::Failed;
    }

    // A newer release exists → mandatory update.
    log_i("OTA: newer release %s — downloading %s", latest.c_str(), bin_url.c_str());
    if (on_start) on_start();

    s_on_progress = on_progress;
    Update.onProgress([](size_t cur, size_t total) {
        if (total && s_on_progress) {
            s_on_progress((uint8_t)((uint64_t)cur * 100ULL / total));
        }
    });

    // Bind the expected MD5 (validated to be 32 hex chars above) so Update.end()
    // rejects a corrupted or tampered binary at the flash layer instead of
    // boot-looping into a half-written app slot. httpUpdate calls
    // Update.begin()/write()/end() internally; setMD5 sets a target hash the
    // end() step compares against the running MD5 of every byte written.
    Update.setMD5(expected_md5.c_str());

    WiFiClientSecure client;
    // Same CA-bundle validation as the manifest fetch — covers the
    // /releases/download redirect to the asset CDN (esp_crt_bundle is
    // host-agnostic). The binary URL was pinned to our release path above.
    client.setCACertBundle(ca_bundle_start);

    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);  // reboot into the new image on success

    t_httpUpdate_return ret = httpUpdate.update(client, bin_url);

    s_on_progress = nullptr;

    switch (ret) {
        case HTTP_UPDATE_OK:
            // Unreachable: rebootOnUpdate restarts before we get here.
            return CheckResult::UpToDate;
        case HTTP_UPDATE_NO_UPDATES:
            return CheckResult::UpToDate;
        case HTTP_UPDATE_FAILED:
        default:
            log_e("OTA: update failed (%d): %s",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
            return CheckResult::Failed;
    }
}

}  // namespace ota
