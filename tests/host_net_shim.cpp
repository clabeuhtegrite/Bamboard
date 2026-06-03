// Minimal host stubs for the handful of non-inline Arduino/ESP symbols that
// firmware/src/net/bambuddy_client.cpp references on its network paths
// (do_get / do_post / camera fetch). The parser unit tests in test_logic.cpp
// only ever call the apply_*_payload methods — pure JSON → struct logic — so
// none of these stubs run; they exist solely to satisfy the linker WITHOUT
// dragging in the simulator's heavyweight libcurl + stb + display shim TU
// (sim/shim/shims.cpp), keeping the test build dependency-light.

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <cstddef>
#include <cstdint>

// --- timing (declared, not inline, in the Arduino.h shim) ------------------
uint32_t millis() { return 0; }
void     delay(uint32_t) {}

// --- global instances the firmware expects (extern in the shims) -----------
WiFiClass WiFi;
EspClass  ESP;

// --- embedded root-CA bundle symbol (firmware/src/net/ca_bundle.h) ---------
// On-device this is the linker-embedded CA set; here a 1-byte stub is enough:
// begin_request() only takes its address on the (never-taken) https path.
const uint8_t ca_bundle_start[1] = {0};

// --- HTTPClient: "no network" stubs ----------------------------------------
bool        HTTPClient::begin(const String&) { return false; }
bool        HTTPClient::begin(WiFiClientSecure&, const String&) { return false; }
void        HTTPClient::addHeader(const String&, const String&) {}
int         HTTPClient::GET() { return -1; }
int         HTTPClient::POST(const String&) { return -1; }
int         HTTPClient::POST(uint8_t*, size_t) { return -1; }
String      HTTPClient::getString() { return String(); }
Stream&     HTTPClient::getStream() { return stream_; }
WiFiClient* HTTPClient::getStreamPtr() { return &stream_; }
int         HTTPClient::getSize() { return -1; }
bool        HTTPClient::connected() { return false; }
String      HTTPClient::errorToString(int) { return String("stub"); }
void        HTTPClient::end() {}
