#pragma once

#include <Arduino.h>

// Bambuddy host validation, split out of main.cpp so the rules can be unit-
// tested on the host (the captive portal saves a host string these gate).
//
// http  -> must be a private IPv4 or an mDNS *.local name (LAN only; the API key
//          travels in clear). https -> any IPv4 or a hostname/domain. An optional
//          :port is allowed either way. Enforced when the portal saves.
namespace netcfg {

bool digits_only(const String& s);
bool parse_ipv4(const String& h, int oct[4]);
bool is_ipv4(const String& h);
bool is_private_ipv4(const String& h);
bool is_mdns_local(const String& h);
bool is_hostname(const String& h);
void split_host_port(const String& in, String& host, String& port);
bool host_valid_for_scheme(const String& host, bool https);

}  // namespace netcfg
