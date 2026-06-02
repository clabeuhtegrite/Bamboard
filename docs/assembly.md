# Assembly

Fifteen minutes with a screwdriver. No soldering, no wiring — everything is
on a single all-in-one PCB.

> **Heads-up — this guide targets v0.3+ (Guition JC4827W543).** The original
> ILI9488 + DevKitC + buttons + WS2812 build is preserved in the `v0.2.x`
> tag if you want to reference it; the firmware on `main` no longer supports
> that hardware.

## Tools

- Phillips screwdriver (size per TomE1337's case)
- USB-C cable (data, not charge-only)

## Step 1 — Print the case

The enclosure is **TomE1337's "ESP32-S3 Klipper Dashboard Case"** (CC BY-NC-SA
4.0), redistributed in [`case/`](../case/). Print the three parts:

- 1× `case.stl` — main body (holds the board + screen)
- 1× `cover.stl` — rear cover
- 1× `stand.stl` — desk stand

PETG is preferred over PLA — the panel runs warm enough that PLA can soften
after a couple of weeks on a sunny desk. 0.4 mm nozzle, 0.2 mm layer, three
perimeters is plenty. **For the designer's recommended profile, orientation
and any heat-set inserts / screws, follow the
[Printables page](https://www.printables.com/model/1716582-esp32-s3-klipper-dashboard-case-with-weather-crypt)** —
it's the source of truth for this case.

## Step 2 — Assemble the enclosure

The Guition JC4827W543 is one rigid PCB carrying the ESP32-S3-WROOM-1, the
4.3" RGB-parallel IPS panel and the GT911 capacitive touch controller, so
there's no wiring to do.

1. Seat the board in `case.stl`, **panel facing the screen opening**, until
   it sits flush against the internal ribs.
2. Close it with `cover.stl`, lining up the USB-C cutout with the board's
   USB-C port and the BOOT / RST windows with the side switches.
3. Drop the assembled body into `stand.stl`.

Fasteners and exact fit are per TomE1337's model — see the
[Printables page](https://www.printables.com/model/1716582-esp32-s3-klipper-dashboard-case-with-weather-crypt)
and [`case/README.md`](../case/README.md). If a board revision is a hair off,
check the model's notes for tolerances.

## Step 3 — Flash the firmware

Plug the USB-C cable into the device and into your computer. Open the
[web installer](https://clabeuhtegrite.github.io/Bamboard/) in Chrome,
Edge or Opera and follow [`flashing.md`](flashing.md). Firefox / Safari /
Linux users can flash `bamboard-factory.bin` with `esptool` instead — see
the same doc.

You only need to flash once. Every subsequent update lands over the air
from GitHub Releases.

## Step 4 — First boot

Within ~5 seconds of plugging the USB-C cable in, the panel lights up.
On the very first boot the device exposes a `Bamboard-setup` Wi-Fi
network — connect to it with your phone or laptop and the captive portal
asks for your Wi-Fi credentials, your Bambuddy URL + API key, your
timezone, your daily-reboot hour, and your interface language. Save and
the device reboots into normal operation.

You're done.
