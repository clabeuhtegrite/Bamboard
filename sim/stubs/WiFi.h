// WiFi.h shim: firmware/src/ui/screens.cpp does `#include <WiFi.h>` for
// the Settings screen. The real Arduino header is gigantic; the sim
// only needs WiFi.localIP() and WiFi.RSSI(), so we forward to the
// minimal WiFiClass defined in our Arduino.h shim.

#pragma once
#include "Arduino.h"
