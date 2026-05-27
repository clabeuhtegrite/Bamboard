#include "led.h"

#include <FastLED.h>

#include "../config.h"

namespace hw {

StatusLed g_led;

static CRGB s_leds[1];

// A small breathing helper. Returns 0..255.
static uint8_t breathe(uint32_t period_ms, uint8_t min_v, uint8_t max_v) {
    uint32_t t = millis() % period_ms;
    float ph   = (float)t / (float)period_ms;            // 0..1
    float s    = 0.5f * (1.0f - cosf(ph * 2.0f * PI));   // 0..1
    return (uint8_t)(min_v + s * (max_v - min_v));
}

void StatusLed::begin() {
    FastLED.addLeds<WS2812B, pins::LED_DIN, GRB>(s_leds, pins::LED_COUNT);
    FastLED.setBrightness(180);
    s_leds[0] = CRGB::Black;
    FastLED.show();
}

void StatusLed::tick(::bambuddy::PrinterState state, bool has_hms_error) {
    using PS = ::bambuddy::PrinterState;

    CRGB c = CRGB::Black;
    if (has_hms_error) {
        uint8_t v = breathe(600, 30, 255);
        c = CRGB(v, 0, 0);
    } else switch (state) {
        case PS::Printing: c = CRGB(0, 0xB7, 0xC3); break;       // teal
        case PS::Paused: {
            uint8_t v = breathe(1400, 30, 220);
            c = CRGB(v, (uint8_t)(v * 0.6f), 0);                 // amber
            break;
        }
        case PS::Finish:  c = CRGB(0x39, 0xD9, 0x8A); break;     // green
        case PS::Failed:
        case PS::Error: {
            uint8_t v = breathe(450, 40, 255);
            c = CRGB(v, 0x10, 0x10);
            break;
        }
        case PS::Offline: c = CRGB(20, 20, 20);                 break;
        case PS::Idle:
        case PS::Prepare:
        default: {
            uint8_t v = breathe(3000, 12, 60);
            c = CRGB(0, v / 2, v);                               // soft blue
            break;
        }
    }
    if (s_leds[0] != c) {
        s_leds[0] = c;
        FastLED.show();
    }
}

}  // namespace hw
