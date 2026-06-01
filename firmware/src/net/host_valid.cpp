#include "host_valid.h"

namespace netcfg {

bool digits_only(const String& s) {
    if (s.length() == 0) return false;
    for (size_t i = 0; i < s.length(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

bool parse_ipv4(const String& h, int oct[4]) {
    int parts = 0, start = 0;
    const int n = h.length();
    for (int i = 0; i <= n; ++i) {
        if (i == n || h[i] == '.') {
            String p = h.substring(start, i);
            if (parts >= 4 || !digits_only(p) || p.length() > 3) return false;
            int v = p.toInt();
            if (v < 0 || v > 255) return false;
            oct[parts++] = v;
            start = i + 1;
        } else if (h[i] < '0' || h[i] > '9') {
            return false;
        }
    }
    return parts == 4;
}

bool is_ipv4(const String& h) { int o[4]; return parse_ipv4(h, o); }

bool is_private_ipv4(const String& h) {
    int o[4];
    if (!parse_ipv4(h, o)) return false;
    if (o[0] == 10) return true;                              // 10.0.0.0/8
    if (o[0] == 192 && o[1] == 168) return true;              // 192.168.0.0/16
    if (o[0] == 172 && o[1] >= 16 && o[1] <= 31) return true; // 172.16.0.0/12
    if (o[0] == 127) return true;                             // loopback
    return false;
}

bool is_mdns_local(const String& h) {
    String l = h; l.toLowerCase();
    return l.length() > 6 && l.endsWith(".local");
}

// RFC-1123-ish hostname: dot-separated labels of [A-Za-z0-9-], 1..63 chars each,
// not starting/ending with '-'.
bool is_hostname(const String& h) {
    if (h.length() == 0 || h.length() > 253) return false;
    int start = 0;
    const int n = h.length();
    for (int i = 0; i <= n; ++i) {
        if (i == n || h[i] == '.') {
            int len = i - start;
            if (len == 0 || len > 63) return false;
            if (h[start] == '-' || h[i - 1] == '-') return false;
            start = i + 1;
        } else {
            char c = h[i];
            bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '-';
            if (!ok) return false;
        }
    }
    return true;
}

// Split "host[:port]" -> host + port ("" when absent). IPv6 literals unsupported.
void split_host_port(const String& in, String& host, String& port) {
    int c = in.indexOf(':');
    if (c >= 0 && in.indexOf(':', c + 1) < 0) {   // exactly one ':'
        host = in.substring(0, c);
        port = in.substring(c + 1);
    } else {
        host = in;
        port = "";
    }
}

bool host_valid_for_scheme(const String& host, bool https) {
    if (https) return is_ipv4(host) || is_hostname(host);
    return is_private_ipv4(host) || is_mdns_local(host);
}

}  // namespace netcfg
