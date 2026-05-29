// Minimal Arduino-compat shim for the desktop LVGL simulator.
//
// firmware/src/ui/{screens,ui}.cpp + LVGL's own lv_tick.c (which is C
// code reached via LV_TICK_CUSTOM_INCLUDE="Arduino.h") all pull this
// header in. Split into a C-callable section for the LVGL tick hook and
// a C++-only section for the String / WiFi / log_* shims used by the
// firmware sources.

#pragma once

#include <stdint.h>

// =============================================================================
// C-callable surface  (visible to LVGL's lv_tick.c)
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

uint32_t millis(void);
void     delay(uint32_t ms);

#ifdef __cplusplus
}  // extern "C"
#endif

// =============================================================================
// C++-only surface  (visible to the firmware sources)
// =============================================================================

#ifdef __cplusplus

#include <cstdio>
#include <cstring>
#include <string>

// ---------- log_* macros ----------------------------------------------------

#ifndef log_i
#define log_i(fmt, ...) std::fprintf(stderr, "[I] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef log_w
#define log_w(fmt, ...) std::fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef log_e
#define log_e(fmt, ...) std::fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef log_d
#define log_d(fmt, ...) std::fprintf(stderr, "[D] " fmt "\n", ##__VA_ARGS__)
#endif

// ---------- Arduino-style String --------------------------------------------
//
// Just enough for firmware/src/ui/* to compile. Wraps std::string but
// adds the numeric constructors — Arduino's `String(int)` formats the
// number, std::string's would build a string of N null bytes.

class String {
   public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    String(int n)                 { fmt_("%d",   n); }
    String(unsigned int n)        { fmt_("%u",   n); }
    String(long n)                { fmt_("%ld",  n); }
    String(unsigned long n)       { fmt_("%lu",  n); }
    String(float f, int dec = 2)  { char b[32]; std::snprintf(b, sizeof(b), "%.*f", dec, f); s_ = b; }

    size_t      length() const { return s_.length(); }
    const char* c_str()  const { return s_.c_str();  }

    String& operator=(const char* s)     { s_ = (s ? s : ""); return *this; }
    String& operator+=(const String& o)  { s_ += o.s_; return *this; }
    String& operator+=(const char* s)    { s_ += (s ? s : ""); return *this; }
    String  operator+(const String& o) const { String r = *this; r += o; return r; }
    String  operator+(const char* s)   const { String r = *this; r += s; return r; }

    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    bool operator==(const char* o)   const { return std::strcmp(s_.c_str(), o ? o : "") == 0; }
    bool operator!=(const char* o)   const { return !(*this == o); }

   private:
    std::string s_;
    template <typename T>
    void fmt_(const char* f, T v) { char b[24]; std::snprintf(b, sizeof(b), f, v); s_ = b; }
};

inline String operator+(const char* a, const String& b) { return String(a) + b; }

// ---------- WiFi singleton (for Settings screen) ---------------------------

class FakeIp {
   public:
    explicit FakeIp(const char* s) : ip_(s) {}
    String toString() const { return ip_; }
   private:
    String ip_;
};

class WiFiClass {
   public:
    FakeIp localIP() const { return FakeIp("192.168.1.42"); }
    int    RSSI()    const { return -58; }
};

extern WiFiClass WiFi;

// ---------- Misc Arduino bits screens.cpp uses ------------------------------

#ifndef LOW
#define LOW  0
#define HIGH 1
#endif

#endif  // __cplusplus
