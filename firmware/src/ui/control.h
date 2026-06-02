// Printer control actions, marshalled OFF the UI/LVGL task.
//
// A control action (pause, stop, clear plate, speed, light, AMS dry…) needs a
// blocking HTTP POST to Bambuddy. Issuing it straight from an LVGL touch
// callback runs it on the UI task — which is watchdog-guarded and drives the
// display — so a slow or unreachable server would freeze the screen for the
// whole round-trip and could trip the task WDT into a reboot. It would also
// race the network task on the REST client's shared state.
//
// So touch handlers ENQUEUE a command here instead. The network task drains the
// queue (net_task → control_process) and runs the POST off the watchdog, then
// reports the outcome to the UI via ui::screens::request_toast (park-and-apply).
//
// enqueue() is implemented in main.cpp on the device and stubbed in the host
// simulator (which never taps the buttons).
#pragma once

#include <stdint.h>

namespace ui::ctrl {

enum Op : uint8_t {
    Speed,        // a = mode (1..4)
    ClearPlate,
    ClearHms,
    Pause,
    Resume,
    Stop,
    Light,        // a = on (0/1)
    DryStart,     // a = AMS unit index, b = temp °C, c = minutes
    DryStop,      // a = AMS unit index
    QueueCancel,  // printer_id field carries the queue-item id
};

// Non-blocking: parks the command for the network task. A full queue simply
// drops the tap (the user can tap again); it never blocks the UI task.
void enqueue(Op op, int printer_id, uint8_t a = 0, uint8_t b = 0, uint16_t c = 0);

}  // namespace ui::ctrl
