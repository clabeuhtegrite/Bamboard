// Host shim for <WiFiClientSecure.h>.
//
// On the device this validates Bambuddy's TLS certificate against the embedded
// root-CA bundle. The sim reaches Bambuddy over libcurl (which uses the host's
// system trust store), so the TLS setters here are no-ops. It derives WiFiClient
// so HTTPClient::begin(client, url) accepts it where the firmware passes a
// secure client for https:// URLs.
#pragma once
#include <WiFiClient.h>
#include <cstddef>
#include <cstdint>

class WiFiClientSecure : public WiFiClient {
   public:
    void setInsecure() {}
    void setCACertBundle(const uint8_t*) {}
    void setCACertBundle(const uint8_t*, size_t) {}
};
