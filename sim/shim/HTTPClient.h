// Host shim for <HTTPClient.h> — backed by libcurl (impl in shims.cpp). Adds
// Cloudflare Access service-token headers from the environment so it can reach
// a Bambuddy behind Cloudflare Access. Without libcurl, requests just fail
// (the UI then renders its empty states — still useful for layout validation).
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <string>
#include <utility>
#include <vector>

#define HTTP_CODE_OK 200

class HTTPClient {
   public:
    bool   begin(const String& url);
    void   addHeader(const String& key, const String& value);
    void   setTimeout(uint32_t) {}
    void   setConnectTimeout(uint32_t) {}
    int    GET();
    int    POST(const String& body);
    int    POST(uint8_t* body, size_t len);
    String getString();
    Stream& getStream();
    WiFiClient* getStreamPtr();
    int    getSize();
    bool   connected();
    String errorToString(int code);
    void   end();

   private:
    int perform(const char* method, const std::string& body);

    std::string url_;
    std::vector<std::pair<std::string, std::string>> hdrs_;
    WiFiClient  stream_;
    long        size_ = -1;
    int         code_ = 0;
};
