# Wiring

There isn't any.

The v0.3 build uses a single all-in-one board (the Guition JC4827W543)
that integrates the ESP32-S3, the 4.3" IPS touch panel and every signal
between them on one PCB. The only thing you plug in is the USB-C cable.

## What used to be wiring

In v0.x there were 12+ Dupont jumpers carrying SPI / I²C / WS2812
signals between a bare ESP32-S3 DevKitC, an ILI9488 display and three
button modules. That whole tree is replaced by the JC4827W543's
internal copper traces.

If you're curious about what the board does internally, the
manufacturer's schematic is published on
[their AliExpress store](https://www.aliexpress.com/wholesale?SearchText=Guition+JC4827W543+ESP32-S3)
and mirrored in various community repos. The short version:

| Signal group        | How it's connected on the board                            |
|---------------------|-------------------------------------------------------------|
| Display (RGB565)    | 16-bit RGB parallel to the ESP32-S3's LCD_CAM peripheral.   |
| Display backlight   | PWM on GPIO 2, controlled by firmware (auto-dim still works).|
| Capacitive touch    | GT911 over I²C — SDA on GPIO 19, SCL on GPIO 20, INT/RST on 18 / 38. |
| USB                 | Native USB on GPIO 19 / 20 (same physical USB-C connector that powers it). |
| BOOT / RST buttons  | Side-mounted tactile switches on the PCB. BOOT is GPIO 0, used by the firmware as the factory-reset trigger when held during power-up. |

## Powering the device

USB-C from any reasonable 5 V / 1 A source. Total draw is ~250 mA peak
(backlight + Wi-Fi TX).

## Sanity-check checklist

- [ ] USB-C cable is **data + power**, not power-only (the cheap white
      cables bundled with phone chargers are a common gotcha — the
      device will boot but PlatformIO won't see it for the first flash).
- [ ] The board's BOOT button is reachable from the outside of the case
      (TomE1337's enclosure exposes it by default).

## Factory reset (wipe all persisted settings)

Hold the **BOOT** button on the side of the PCB while the device powers
on. The firmware sees the GPIO 0 line pulled low at boot and clears NVS
before re-opening the captive portal. This wipes everything stored on the
device — Wi-Fi + Bambuddy creds, timezone, daily-reboot hour, brightness
and interface language — not just the network credentials.

The old "hold PREV at boot" trick is gone with the rest of the
standalone buttons — same idea, different physical input.
