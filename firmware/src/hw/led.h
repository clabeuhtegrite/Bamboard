// Single WS2812 RGB status LED.
//
// Provides "ambient" patterns mapped to printer state:
//   - Idle:           soft blue breathing
//   - Printing:       solid teal (matches UI accent)
//   - Paused:         slow yellow pulse
//   - Finish:         green
//   - Failed / Error: red, fast pulse
//   - Offline:        dim grey

#pragma once

#include <Arduino.h>
#include "../net/bambuddy_client.h"

namespace hw {

class StatusLed {
   public:
    void begin();
    // Drives the LED based on a printer state. Should be called regularly
    // (e.g. every 30 ms from the UI tick).
    void tick(::bambuddy::PrinterState state, bool has_hms_error);
};

extern StatusLed g_led;

}  // namespace hw
