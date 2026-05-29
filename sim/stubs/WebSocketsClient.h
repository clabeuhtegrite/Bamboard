// Minimal WebSocketsClient stub for the desktop sim.
//
// `firmware/src/net/bambuddy_ws.h` includes <WebSocketsClient.h> from the
// links2004 lib and embeds a WebSocketsClient instance as a member of
// WsClient. The sim doesn't actually talk to a WebSocket, but the
// constructor still has to compile — so we provide just enough of the
// surface (the WStype_t enum + a no-op class) for the header to parse.

#pragma once

#include <cstddef>
#include <cstdint>

enum WStype_t {
    WStype_ERROR        = 0,
    WStype_DISCONNECTED = 1,
    WStype_CONNECTED    = 2,
    WStype_TEXT         = 3,
    WStype_BIN          = 4,
    WStype_FRAGMENT_TEXT_START = 5,
    WStype_FRAGMENT_BIN_START  = 6,
    WStype_FRAGMENT            = 7,
    WStype_FRAGMENT_FIN        = 8,
    WStype_PING                = 9,
    WStype_PONG                = 10,
};

class WebSocketsClient {
   public:
    WebSocketsClient()  = default;
    ~WebSocketsClient() = default;

    void loop() {}
    void disconnect() {}
    void begin   (const char*, uint16_t, const char*) {}
    void beginSSL(const char*, uint16_t, const char*) {}
    bool sendTXT (const char*) { return true; }
    void setReconnectInterval(unsigned long) {}

    template <typename F>
    void onEvent(F&&) {}
};
