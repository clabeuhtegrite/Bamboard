# Assembly

Fifteen minutes with a screwdriver. No soldering, no wiring — everything is
on a single all-in-one PCB.

> **Heads-up — this guide targets v0.3+ (Guition JC4827W543).** The original
> ILI9488 + DevKitC + buttons + WS2812 build is preserved in the `v0.2.x`
> tag if you want to reference it; the firmware on `main` no longer supports
> that hardware.

## Tools

- Phillips PH0 or PH1 screwdriver
- USB-C cable (data, not charge-only)

## Step 1 — Print the case

See [`case/README.md`](../case/README.md). You need:

- 1× `front_shell`
- 1× `back_shell`

PETG is preferred over PLA — the panel runs warm enough that PLA can soften
after a couple of weeks on a sunny desk. 0.4 mm nozzle, 0.2 mm layer,
three perimeters is plenty.

## Step 2 — Drop the board into the front shell

The Guition JC4827W543 is one rigid PCB carrying the ESP32-S3-WROOM-1, the
4.3" RGB-parallel IPS panel and the GT911 capacitive touch controller. No
external wiring.

1. Hold the front shell display-side down on the desk.
2. Lower the board into the pocket, **panel facing the shell**. The PCB
   outline should sit flush against the four corner ribs; the chamfered
   edge of the shell wraps around the bezel so the case reads as one piece.
3. If the fit is too tight (some board revisions are ±0.5 mm off), don't
   force it — bump `board_w` / `board_h` at the top of
   [`case/bamboard.scad`](../case/bamboard.scad) by the missing millimetre
   and re-print the front shell only.

## Step 3 — Clip the back shell on

1. Line up the back shell so the USB-C cutout sits over the board's USB-C
   port and the BOOT / RST button windows match the side switches.
2. Press the two shells together — the corner posts should slot through
   the matching front-shell bosses with very little play.

## Step 4 — Drive four M3 × 6 mm screws

Self-tapping M3 × 6 mm into the four corner posts. Snug, don't
over-tighten — the bosses are 1.6 mm thick on v0.4 and will strip if you
lean on them. (v0.3 used M3 × 8 mm; if you have leftover screws from a
v0.3 build, that's fine, but the head will sit slightly proud.)

## Step 5 — Flash the firmware

Plug the USB-C cable into the device and into your computer. Open the
[web installer](https://clabeuhtegrite.github.io/Bamboard/) in Chrome,
Edge or Opera and follow [`flashing.md`](flashing.md). Firefox / Safari /
Linux users can flash `bamboard-factory.bin` with `esptool` instead — see
the same doc.

You only need to flash once. Every subsequent update lands over the air
from GitHub Releases.

## Step 6 — First boot

Within ~5 seconds of plugging the USB-C cable in, the panel lights up.
On the very first boot the device exposes a `Bamboard-setup` Wi-Fi
network — connect to it with your phone or laptop and the captive portal
asks for your Wi-Fi credentials, your Bambuddy URL + API key, your
timezone, your daily-reboot hour, and your interface language. Save and
the device reboots into normal operation.

You're done.
