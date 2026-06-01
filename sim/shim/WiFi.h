// Host shim for <WiFi.h> — always "connected" so the REST client proceeds.
#pragma once
#include <Arduino.h>

enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_STA = 1, WIFI_AP = 2 };

class IPAddress {
   public:
    uint8_t o[4];
    IPAddress(uint8_t a = 192, uint8_t b = 168, uint8_t c = 1, uint8_t d = 42) {
        o[0] = a; o[1] = b; o[2] = c; o[3] = d;
    }
    uint8_t operator[](int i) const { return o[i & 3]; }
    String toString() const {
        char b[20]; snprintf(b, sizeof(b), "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
        return String(b);
    }
};

class WiFiClass {
   public:
    int  status() { return WL_CONNECTED; }
    int  RSSI()   { return -55; }
    IPAddress localIP() { return IPAddress(); }
    void mode(int) {}
    void setHostname(const char*) {}
    void begin() {}
    void begin(const char*, const char*) {}
    void disconnect(bool = false, bool = false) {}
};
extern WiFiClass WiFi;
