// Optional: hardcode credentials at build time instead of going through the
// captive portal. Copy this file to `secrets.h` (which is gitignored) and
// fill in your values. If `secrets.h` is present at compile time, the
// firmware will skip the captive portal on first boot.
//
// In normal operation you do NOT need this file — first boot exposes a Wi-Fi
// AP called "Bamboard-setup" where you configure everything.

#pragma once

#define BAMBOARD_WIFI_SSID     ""
#define BAMBOARD_WIFI_PASSWORD ""

// Full URL to your Bambuddy instance, e.g. "http://192.168.1.42:8000"
#define BAMBOARD_BAMBUDDY_URL  ""

// API key created in Bambuddy → Settings → API Keys
#define BAMBOARD_API_KEY       ""
