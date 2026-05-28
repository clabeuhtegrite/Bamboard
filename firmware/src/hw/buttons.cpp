#include "buttons.h"

#include "../config.h"

namespace hw {

Buttons g_buttons;

void Buttons::begin() {
    pins_[(uint8_t)Btn::Prev] = pins::BTN_PREV;
    pins_[(uint8_t)Btn::Ok]   = pins::BTN_OK;
    pins_[(uint8_t)Btn::Next] = pins::BTN_NEXT;
    for (uint8_t i = 0; i < (uint8_t)Btn::_Count; ++i) {
        pinMode(pins_[i], INPUT_PULLUP);
    }
}

void Buttons::push(ButtonEvent e) {
    uint8_t next = (head_ + 1) % Q;
    if (next == tail_) {
        // Ring full — drop oldest by advancing tail. We'd rather lose the
        // oldest event than block the input task.
        tail_ = (tail_ + 1) % Q;
    }
    q_[head_] = e;
    head_ = next;
}

bool Buttons::next_event(ButtonEvent& out) {
    if (head_ == tail_) return false;
    out = q_[tail_];
    tail_ = (tail_ + 1) % Q;
    return true;
}

void Buttons::update() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < (uint8_t)Btn::_Count; ++i) {
        State& s   = states_[i];
        bool pressed = (digitalRead(pins_[i]) == LOW);  // active-low

        if (pressed != s.raw_pressed) {
            s.raw_pressed = pressed;
            s.last_change_ms = now;
        }

        if (pressed != s.stable_pressed &&
            (now - s.last_change_ms) >= buttons::DEBOUNCE_MS) {
            s.stable_pressed = pressed;
            if (pressed) {
                s.press_start_ms = now;
                s.long_fired     = false;
                s.next_repeat_ms = now + buttons::REPEAT_AFTER_MS;
                last_activity_ms_ = now;
            } else {
                // Release. If we never fired a long press, fire a short Press
                // and also fire DoublePress when this release closes a pair
                // that started within the double-click window. Consumers that
                // only care about single clicks can ignore DoublePress; ones
                // that want the gesture see the pair "Press, Press, DoublePress"
                // and choose which to act on.
                if (!s.long_fired) {
                    push({(Btn)i, BtnEvent::Press});
                    if (s.last_release_ms != 0 &&
                        (now - s.last_release_ms) <= buttons::DOUBLE_CLICK_MS) {
                        push({(Btn)i, BtnEvent::DoublePress});
                        s.last_release_ms = 0;   // don't chain into a triple
                    } else {
                        s.last_release_ms = now;
                    }
                }
                last_activity_ms_ = now;
            }
        }

        if (s.stable_pressed) {
            if (!s.long_fired && (now - s.press_start_ms) >= buttons::LONG_PRESS_MS) {
                s.long_fired = true;
                push({(Btn)i, BtnEvent::LongPress});
            }
            if (s.long_fired && now >= s.next_repeat_ms) {
                push({(Btn)i, BtnEvent::Repeat});
                s.next_repeat_ms = now + buttons::REPEAT_MS;
            }
        }
    }
}

}  // namespace hw
