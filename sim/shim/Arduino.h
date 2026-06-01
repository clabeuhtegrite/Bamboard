// Host shim for <Arduino.h> — just enough of the Arduino core for the Bamboard
// UI + Bambuddy REST client to compile and run on a PC. NOT a faithful Arduino.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

// ---- timing ---------------------------------------------------------------
uint32_t millis();
void     delay(uint32_t ms);

// ---- logging (firmware uses log_i/log_w/log_e/log_d) ----------------------
#define log_e(fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#define log_w(fmt, ...) fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#define log_i(fmt, ...) fprintf(stderr, "[I] " fmt "\n", ##__VA_ARGS__)
#define log_d(fmt, ...) fprintf(stderr, "[D] " fmt "\n", ##__VA_ARGS__)

// ---- Arduino String over std::string --------------------------------------
class String {
   public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    String(char c) : s_(1, c) {}
    String(int v) : s_(std::to_string(v)) {}
    String(unsigned v) : s_(std::to_string(v)) {}
    String(long v) : s_(std::to_string(v)) {}
    String(unsigned long v) : s_(std::to_string(v)) {}
    String(uint16_t v) : s_(std::to_string((unsigned)v)) {}
    String(double v, int decimals = 2) {
        char b[32]; snprintf(b, sizeof(b), "%.*f", decimals, v); s_ = b;
    }

    const char* c_str() const { return s_.c_str(); }
    size_t length() const { return s_.size(); }
    char operator[](size_t i) const { return i < s_.size() ? s_[i] : '\0'; }

    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o) { s_ += (o ? o : ""); return *this; }

    // concat() overloads — ArduinoJson's Arduino-String writer needs these.
    String& concat(const String& o) { s_ += o.s_; return *this; }
    String& concat(const char* p) { s_ += (p ? p : ""); return *this; }
    String& concat(const char* p, size_t n) { s_.append(p, n); return *this; }
    String& concat(char c) { s_ += c; return *this; }
    bool reserve(size_t n) { s_.reserve(n); return true; }

    bool startsWith(const char* p) const { return s_.rfind(p, 0) == 0; }
    bool endsWith(const char* p) const {
        std::string q(p ? p : ""); if (q.size() > s_.size()) return false;
        return s_.compare(s_.size() - q.size(), q.size(), q) == 0;
    }
    int indexOf(char c) const { auto p = s_.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c, int from) const { auto p = s_.find(c, from); return p == std::string::npos ? -1 : (int)p; }
    String substring(int a) const { return String(s_.substr(a < 0 ? 0 : a)); }
    String substring(int a, int b) const {
        if (a < 0) a = 0; if (b < a) b = a;
        return String(s_.substr(a, b - a));
    }
    void remove(int idx) { if (idx >= 0 && (size_t)idx < s_.size()) s_.erase(idx); }
    void remove(int idx, int count) { if (idx >= 0 && (size_t)idx < s_.size()) s_.erase(idx, count); }
    void trim() {
        size_t a = s_.find_first_not_of(" \t\r\n");
        size_t b = s_.find_last_not_of(" \t\r\n");
        s_ = (a == std::string::npos) ? "" : s_.substr(a, b - a + 1);
    }
    long toInt() const { return strtol(s_.c_str(), nullptr, 10); }

    bool operator==(const char* o) const { return s_ == (o ? o : ""); }
    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator!=(const char* o) const { return !(*this == o); }
    bool operator!=(const String& o) const { return s_ != o.s_; }

    const std::string& std_str() const { return s_; }

   private:
    std::string s_;
};

inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b)   { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b)   { String r(a); r += b; return r; }
inline String operator+(const String& a, int b)           { String r(a); r += String(b); return r; }
inline String operator+(const String& a, unsigned b)      { String r(a); r += String(b); return r; }
inline String operator+(const String& a, unsigned long b) { String r(a); r += String(b); return r; }
inline String operator+(const String& a, char b)          { String r(a); r += String(b); return r; }

// ---- Print / Printable (ArduinoJson references these in its Arduino path) --
class Print {
   public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* b, size_t n) {
        size_t i = 0; for (; i < n; ++i) write(b[i]); return i;
    }
    size_t print(const char* s) { size_t n = 0; while (s && *s) { write((uint8_t)*s++); ++n; } return n; }
};
class Printable {
   public:
    virtual ~Printable() {}
    virtual size_t printTo(Print&) const = 0;
};

// ---- ESP info object -------------------------------------------------------
struct EspClass {
    uint32_t getFreeHeap()  { return 200u * 1024u; }
    uint32_t getFreePsram() { return 4u * 1024u * 1024u; }
    void     restart()      { exit(0); }
};
extern EspClass ESP;

// ---- SNTP-ish helpers used by the UI --------------------------------------
inline bool getLocalTime(struct tm* info, uint32_t = 0) {
    time_t t = time(nullptr);
    localtime_r(&t, info);
    return true;
}

template <typename T> T _min(T a, T b) { return a < b ? a : b; }

// ---- Stream (ArduinoJson + the camera fetch read through this) ------------
class Stream {
   public:
    virtual ~Stream() {}
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual size_t readBytes(uint8_t* buf, size_t len) {
        size_t n = 0;
        while (n < len) { int c = read(); if (c < 0) break; buf[n++] = (uint8_t)c; }
        return n;
    }
    size_t readBytes(char* buf, size_t len) { return readBytes((uint8_t*)buf, len); }
};
