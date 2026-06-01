#include "bambuddy_ws.h"

#include <ArduinoJson.h>

#include "bambuddy_client.h"

namespace bambuddy {

WsClient g_ws;

// Decompose "http(s)://host[:port][/path]" into host + port + TLS flag.
// Tolerates the trailing slash that the captive portal leaves in by chance.
static void parse_ws_url(const String& base_url,
                         String& host_out,
                         uint16_t& port_out,
                         bool& secure_out) {
    String u = base_url;
    secure_out = false;
    if (u.startsWith("https://")) { secure_out = true; u = u.substring(8); port_out = 443; }
    else if (u.startsWith("http://"))                  { u = u.substring(7); port_out = 80;  }
    else                                                {                    port_out = 80;  }

    int slash = u.indexOf('/');
    if (slash >= 0) u = u.substring(0, slash);

    int colon = u.indexOf(':');
    if (colon >= 0) {
        long p = u.substring(colon + 1).toInt();
        if (p > 0 && p < 65536) port_out = (uint16_t)p;
        u = u.substring(0, colon);
    }
    host_out = u;
}

void WsClient::apply_url(const String& base_url) {
    String new_host;
    uint16_t new_port = 0;
    bool new_secure = false;
    parse_ws_url(base_url, new_host, new_port, new_secure);

    if (configured_ &&
        new_host == host_ && new_port == port_ && new_secure == secure_) {
        return;   // nothing to do — same target, leave the live socket alone
    }

    host_   = new_host;
    port_   = new_port;
    secure_ = new_secure;

    if (configured_) client_.disconnect();

    if (secure_) client_.beginSSL(host_.c_str(), port_, "/ws");
    else         client_.begin   (host_.c_str(), port_, "/ws");

    // Application-level pings handle keepalive (see Bambuddy's ws handler),
    // so the protocol-level heartbeat is left off to avoid double-pinging.
    client_.setReconnectInterval(5000);
    configured_ = true;
}

void WsClient::begin(const String& base_url) {
    client_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handle_event(type, payload, length);
    });
    apply_url(base_url);
}

void WsClient::set_credentials(const String& base_url) {
    apply_url(base_url);
}

void WsClient::loop() {
    if (!configured_) return;
    client_.loop();
    if (connected_ && millis() - last_ping_ms_ > 25000UL) {
        client_.sendTXT("{\"type\":\"ping\"}");
        last_ping_ms_ = millis();
    }
}

void WsClient::handle_event(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            connected_    = true;
            last_ping_ms_ = millis();
            log_i("WS connected to %s:%u", host_.c_str(), (unsigned)port_);
            break;
        case WStype_DISCONNECTED:
            connected_ = false;
            log_i("WS disconnected");
            break;
        case WStype_TEXT:
            handle_text(payload, length);
            break;
        case WStype_ERROR:
            connected_ = false;
            log_w("WS error");
            break;
        default:
            break;
    }
}

void WsClient::handle_text(uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        log_w("WS json parse: %s", err.c_str());
        return;
    }
    const char* msg_type = doc["type"] | "";
    if (!strcmp(msg_type, "printer_status")) {
        int pid = doc["printer_id"] | -1;
        if (pid >= 0) {
            // doc["data"] borrows from the local doc — safe because
            // apply_status_payload finishes copying before we return.
            g_client.apply_status_payload(pid, doc["data"]);
        }
    }
    // pong / background_dispatch / unknown — silently ignored.
}

}  // namespace bambuddy
