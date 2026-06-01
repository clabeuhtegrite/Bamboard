// Bambuddy WebSocket push-event client.
//
// Bambuddy exposes a single unauthenticated WebSocket at `/ws` (see
// backend/app/api/routes/websocket.py in the upstream repo). On connect the
// server emits one `printer_status` frame per known printer, then keeps
// pushing fresh `printer_status` frames whenever a printer's underlying
// state changes via MQTT. Switching to push cuts HMS-alert latency from a
// ~2 s polling window to "as fast as the WS hop", which is the whole point
// of the upstream `/ws` endpoint.
//
// The wire format we care about:
//   { "type": "printer_status",
//     "printer_id": <int>,
//     "data": <PrinterStatus> }            ← same shape as GET /status
//   { "type": "pong" }                     ← reply to our `{"type":"ping"}`
//   { "type": "background_dispatch", ... } ← print-queue progress (ignored here)
//
// We keep REST polling enabled as a safety net (see main.cpp) — the WS path
// is fast but lossy under flaky Wi-Fi, and a missed frame would otherwise
// leave the UI stale until the user navigated.

#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

namespace bambuddy {

class WsClient {
   public:
    // Bambuddy's /ws endpoint is unauthenticated (see the upstream
    // websocket route), so we deliberately take no API key here — the REST
    // client owns the key. begin()/set_credentials() keep a url-only
    // signature so a future authenticated handshake is an additive change.
    void begin(const String& base_url);

    // Re-bind to a (possibly new) Bambuddy URL. Tears the current socket
    // down if the host changed; same host stays connected.
    void set_credentials(const String& base_url);

    // Drive the underlying WebSocketsClient FSM + app-level keepalive.
    // Must be called frequently from the network task (≥ once / second).
    void loop();

    bool is_connected() const { return connected_; }

   private:
    void apply_url(const String& base_url);
    void handle_event(WStype_t type, uint8_t* payload, size_t length);
    void handle_text(uint8_t* payload, size_t length);

    WebSocketsClient client_;
    String   host_;
    uint16_t port_      = 0;
    bool     secure_    = false;
    bool     configured_ = false;
    bool     connected_  = false;
    uint32_t last_ping_ms_ = 0;
};

extern WsClient g_ws;

}  // namespace bambuddy
