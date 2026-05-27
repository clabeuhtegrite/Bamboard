// Three-button input with debouncing, long press and auto-repeat.
//
// Wiring: each button shorts its GPIO to GND, with the ESP32 internal
// pull-up enabled. No external components.

#pragma once

#include <Arduino.h>

namespace hw {

enum class Btn : uint8_t { Prev = 0, Ok = 1, Next = 2, _Count = 3 };

enum class BtnEvent : uint8_t {
    None,
    Press,        // short press, fired on release
    LongPress,    // fired once when held past threshold
    Repeat,       // fired periodically while held (after LongPress)
};

struct ButtonEvent {
    Btn      btn;
    BtnEvent ev;
};

class Buttons {
   public:
    void begin();

    // Poll once per loop iteration. Pushes events into an internal ring.
    void update();

    // Pop the oldest pending event. Returns false if the ring is empty.
    bool next_event(ButtonEvent& out);

    // True if the user touched any control since `since_ms`.
    uint32_t last_activity_ms() const { return last_activity_ms_; }

   private:
    struct State {
        bool     raw_pressed    = false;
        bool     stable_pressed = false;
        uint32_t last_change_ms = 0;
        uint32_t press_start_ms = 0;
        bool     long_fired     = false;
        uint32_t next_repeat_ms = 0;
    };

    State states_[(uint8_t)Btn::_Count];
    uint8_t pins_[(uint8_t)Btn::_Count];

    // Tiny event ring (lock-free, single producer/consumer).
    static constexpr uint8_t Q = 16;
    ButtonEvent q_[Q];
    volatile uint8_t head_ = 0;
    volatile uint8_t tail_ = 0;

    uint32_t last_activity_ms_ = 0;

    void push(ButtonEvent e);
};

extern Buttons g_buttons;

}  // namespace hw
