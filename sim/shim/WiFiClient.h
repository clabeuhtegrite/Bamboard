// Host shim for <WiFiClient.h> — a Stream-backed read buffer that the HTTP
// shim fills with a response body.
#pragma once
#include <Arduino.h>
#include <string>

class WiFiClient : public Stream {
   public:
    int available() override { return (int)(buf_.size() - pos_); }
    int read() override { return pos_ < buf_.size() ? (uint8_t)buf_[pos_++] : -1; }
    int peek() override { return pos_ < buf_.size() ? (uint8_t)buf_[pos_] : -1; }
    bool connected() { return pos_ < buf_.size(); }
    void sim_reset(const std::string& b) { buf_ = b; pos_ = 0; }

   private:
    std::string buf_;
    size_t      pos_ = 0;
};
