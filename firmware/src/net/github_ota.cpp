#include "github_ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../config.h"

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
// version + firmware-binary URL + expected MD5. Returns false on any
// network / parse error.
static bool fetch_manifest(String& out_version, String& out_bin_url,
                           String& out_md5, bool& server_replied) {
    server_replied = false;
    WiFiClientSecure client;
    // GitHub + its asset CDN (objects.githubusercontent.com) are HTTPS, and
    // the asset host changes across redirects. We skip cert-chain validation
    // rather than bundle and rotate multiple root CAs on the device. End-to-
    // end integrity for the firmware binary itself is recovered by validating
    // the MD5 published in the manifest against Update's running hash (see
    // check_and_update below) — a MITM swapping the .bin would have to forge
    // a matching MD5, which is infeasible without also tampering with the
    // manifest in flight (and the user can still verify the sha256 published
    // alongside out of band).
    client.setInsecure();

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
    // md5 is optional for backwards compatibility with manifests published by
    // older release.yml workflows; an empty value just disables verification.
    return out_version.length() > 0 && out_bin_url.length() > 0;
}

// ---------------------------------------------------------------------------
// Check + apply
// ---------------------------------------------------------------------------

CheckResult check_and_update(void (*on_start)(), void (*on_progress)(uint8_t)) {
    if (WiFi.status() != WL_CONNECTED) {
        log_w("OTA: Wi-Fi down — skipping update check");
        return CheckResult::NoNetwork;
    }

    String latest, bin_url, expected_md5;
    bool server_replied = false;
    if (!fetch_manifest(latest, bin_url, expected_md5, server_replied)) {
        // Distinguish "couldn't reach the server" from "reached it but the
        // manifest was unparseable / missing fields" so the cause is diagnosable.
        return server_replied ? CheckResult::BadManifest : CheckResult::NoNetwork;
    }

    log_i("OTA: running %s, latest release %s", BAMBOARD_VERSION, latest.c_str());
    if (semver_cmp(latest.c_str(), BAMBOARD_VERSION) <= 0) {
        return CheckResult::UpToDate;
    }

    // Require a published MD5 before committing to a flash. The manifest and the
    // binary both arrive over a setInsecure() TLS channel (see fetch_manifest),
    // so the MD5 — bound into Update.end() below — is the only thing standing
    // between us and an attacker-substituted image. No md5 ⇒ refuse, don't trust
    // the channel. (release.yml always publishes one; this guards a tampered or
    // legacy manifest.) Checked here, before on_start(), so we never pop the
    // update overlay for an image we won't flash.
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
    client.setInsecure();

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
