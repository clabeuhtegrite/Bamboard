# Wiring

All connections are made with female-female Dupont jumpers. No soldering is
required as long as you buy the pre-soldered ESP32-S3 DevKitC, the
pre-soldered display board, and the pre-soldered WS2812 module.

## Pinout map (ESP32-S3 DevKitC-1)

```
                ┌───────────────────────────────────────────┐
                │              ESP32-S3 DevKitC-1           │
                │                                           │
       USB-C ───┤ USB                                       │
                │                                           │
        GND ────┤ GND                       GPIO 4 ────────►┤ → Button PREV
        5V  ────┤ 5V                        GPIO 5 ────────►┤ → Button OK
        3V3 ────┤ 3V3                       GPIO 6 ────────►┤ → Button NEXT
                │                           GPIO 7 ────────►┤ → WS2812 DIN
                │                           GPIO 9 ────────►┤ → Display RST
                │                           GPIO 10 ───────►┤ → Display CS
                │                           GPIO 11 ───────►┤ → Display SDA/MOSI
                │                           GPIO 12 ───────►┤ → Display SCK
                │                           GPIO 13 ───────►┤ → Display SDO/MISO
                │                           GPIO 14 ───────►┤ → Display DC
                │                           GPIO 21 ───────►┤ → Display BL
                └───────────────────────────────────────────┘
```

## Display (ILI9488 SPI)

| Display pin   | ESP32-S3 pin | Notes |
|---------------|--------------|-------|
| VCC           | 5V           | Some boards accept 3V3; check yours. |
| GND           | GND          |       |
| CS            | GPIO 10      |       |
| RESET         | GPIO 9       |       |
| DC / RS       | GPIO 14      |       |
| SDI / MOSI    | GPIO 11      |       |
| SCK           | GPIO 12      |       |
| LED / BL      | GPIO 21      | PWM-controlled by firmware (auto-dim). |
| SDO / MISO    | GPIO 13      | Optional — only used if the firmware reads back from the panel. |
| T_*           | —            | Touch pins are unused; leave disconnected. |

> **If colours look wrong** (red/blue swapped), edit
> `firmware/include/User_Setup.h` and toggle `TFT_RGB_ORDER` between
> `TFT_RGB` and `TFT_BGR`. The default is `TFT_BGR` because most cheap
> 4″ ILI9488 panels are wired that way.

## Buttons

Each button shorts its GPIO to GND. The internal pull-up is enabled in
firmware, so no external resistor is needed.

```
  GPIO 4 ───┐
            │
          [SW] PREV
            │
  GND ──────┘

  Same for OK (GPIO 5) and NEXT (GPIO 6).
```

If you wire the buttons on a small piece of perfboard inside the case, run
a single GND wire to all three commons.

## Status LED (WS2812B module)

| Module pin | ESP32-S3 pin | Notes |
|------------|--------------|-------|
| VCC / 5V   | 5V           | Most modules also accept 3V3; either works for a single LED. |
| GND        | GND          |       |
| DIN        | GPIO 7       |       |

If you have a 3-LED module instead of a single one, increase
`pins::LED_COUNT` in `firmware/src/config.h` to match.

## Power

Power the whole device from the ESP32-S3's USB-C port. The display draws
~120 mA on full backlight, the LED ~10 mA, the MCU peaks around 200 mA on
Wi-Fi TX — well within USB 2.0 budget.

## Sanity-check checklist

- [ ] USB-C cable is **data + power**, not power-only (a surprisingly common gotcha).
- [ ] Display VCC is connected to **5V** (a 3V3-only build will appear very dim).
- [ ] All three buttons have their **GND leg connected**.
- [ ] WS2812 DIN goes to GPIO 7 and **not** to 5V (would blow the data line).
- [ ] No GPIO is shared between two devices.
