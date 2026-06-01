// Host shim for <WebSocketsClient.h> — no-op (the sim validates via REST; live
// WS push isn't needed to render screens). Never connects, so is_connected()
// stays false and the firmware uses its REST safety net.
#pragma once
#include <Arduino.h>
#include <functional>

typedef enum {
    WStype_ERROR, WStype_DISCONNECTED, WStype_CONNECTED, WStype_TEXT,
    WStype_BIN, WStype_FRAGMENT_TEXT_START, WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT, WStype_FRAGMENT_FIN, WStype_PING, WStype_PONG
} WStype_t;

class WebSocketsClient {
   public:
    typedef std::function<void(WStype_t, uint8_t*, size_t)> Cb;
    void onEvent(Cb cb) { cb_ = cb; (void)cb_; }
    void begin(const char*, uint16_t, const char*) {}
    void beginSSL(const char*, uint16_t, const char*) {}
    void disconnect() {}
    void setReconnectInterval(uint32_t) {}
    void setExtraHeaders(const char* = nullptr) {}
    void loop() {}
    bool sendTXT(const char*) { return true; }
    bool sendTXT(const String&) { return true; }

   private:
    Cb cb_;
};
