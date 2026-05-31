# Bill of materials

Indicative prices in EUR, sourced from AliExpress at the time of writing.
Local electronics stores (TME, Mouser, Reichelt) will of course be more
expensive but will deliver in days, not weeks.

The build collapsed in v0.3 to a single all-in-one board (display + touch
+ ESP32-S3 on one PCB), so the parts list is short and zero-soldering is
trivial — there's literally one cable to plug in.

| # | Part                                                  | Qty | ~Price | Notes |
|---|-------------------------------------------------------|-----|--------|-------|
| 1 | Guition **JC4827W543** all-in-one board               | 1   | 16 €   | ESP32-S3-WROOM-1 (8 MB PSRAM / 4 MB flash) + 4.3" IPS 480×272 + capacitive touch (GT911) + USB-C native. Sometimes labelled "Guition ESP32-S3 4.3 inch IPS LCD touch module". |
| 2 | USB-C cable (data, not power-only)                    | 1   | 2 €    | For flashing and powering the device. The thin white cables bundled with phone chargers are often charge-only — look for "data" / "USB 2.0" in the listing. |
| 3 | M3 × 8 mm self-tapping screws                         | 4   | 0.50 € | To clip the case halves together. The case is parametric — you can swap for M2.5 by editing `case/bamboard.scad`. |
| 4 | PLA / PETG filament                                   | ~70 g | 1.10 € | For the printed enclosure. PETG is preferred since the board runs slightly warm. |

**Total: ~20 €**

## Where to buy (AliExpress)

Two search links per item — they're search queries, not product URLs,
so they don't rot when a specific listing disappears. Every part stays
compatible with the no-soldering build: the Guition board has every
component already integrated, and the case ships together with screws.

### 1. Guition JC4827W543 (ESP32-S3 + 4.3" touch)

- [Search: "Guition ESP32-S3 4.3 inch IPS touch"](https://www.aliexpress.com/wholesale?SearchText=Guition+ESP32-S3+4.3+inch+IPS+touch)
- [Search: "JC4827W543 ESP32-S3"](https://www.aliexpress.com/wholesale?SearchText=JC4827W543+ESP32-S3)

What to verify in the listing photo:

- **480 × 272** resolution (not 800 × 480 — that's the bigger sibling
  JC8048W550 which won't fit the case as-is).
- **8 MB PSRAM, 4 MB flash** (sometimes written `N4R8`).
- **Capacitive touch** (GT911 controller) — not resistive.
- USB-C connector visible on the side of the PCB.
- A clearly visible **BOOT** button on the side. The firmware uses it
  as the factory-reset trigger when held during power-up.

### 2. USB-C cable

- [Search: "USB C cable data sync 1m"](https://www.aliexpress.com/wholesale?SearchText=USB+C+cable+data+sync+1m)
- [Search: "USB-C 2.0 data cable 1m"](https://www.aliexpress.com/wholesale?SearchText=USB-C+2.0+data+cable+1m)

The board draws ~250 mA peak (display backlight + Wi-Fi TX), well within
USB 2.0 budget. Any decent 5 V / 1 A phone charger plus a data cable
will run it.

### 3. M3 × 8 mm self-tapping screws

- [Search: "M3 8mm self tapping screw black"](https://www.aliexpress.com/wholesale?SearchText=M3+8mm+self+tapping+screw+black)
- [Search: "M3 PA self-tap screw 8mm assortment"](https://www.aliexpress.com/wholesale?SearchText=M3+PA+self-tap+screw+8mm+assortment)

Assortment kits at ~3 € give you M2 through M5 in 4 to 16 mm lengths. If
you'd rather buy exactly what's needed, single-spec packs run ~1 €.

### 4. PLA / PETG filament

- [Search: "PLA filament 1.75mm 1kg"](https://www.aliexpress.com/wholesale?SearchText=PLA+filament+1.75mm+1kg)
- [Search: "PETG filament 1.75mm 1kg"](https://www.aliexpress.com/wholesale?SearchText=PETG+filament+1.75mm+1kg)

You probably already own a partial spool. ~70 g covers the case.

## Why these parts?

- **JC4827W543, not a bare ESP32 + external display** — collapsing into
  a single board eliminates 100 % of the wiring, every soldering
  question and every "did I get the right SPI mode?" config. The board
  costs about what the bare ESP32-S3 DevKitC + ILI9488 SPI combo used to
  cost separately. The trade-off is the resolution (480 × 272 vs the old
  480 × 320 SPI panel), but the touch interaction makes that visible
  pixel difference irrelevant.
- **4.3" IPS, not OLED** — sustained brightness for a desk monitor that
  may sit in indirect sunlight, and IPS doesn't burn in like OLED would
  with the always-on status header.
- **Capacitive touch, not resistive** — single-finger tap on a glove
  isn't a use case here; capacitive feels noticeably nicer and the
  GT911 controller is well-supported by every LVGL board config in the
  wild.
- **Built-in BOOT button** — used by the firmware as the factory-reset
  gesture. The old PREV-at-boot reset trigger is gone with the rest of
  the standalone buttons.

## What we deliberately left out

- **Status LED** — the panel itself is the status indicator now. The
  HMS-alert pulse runs as a full-screen red overlay instead of a
  WS2812.
- **External buttons** — replaced by on-screen touch controls. The only
  physical switch left in the BOM is the BOOT button that's already on
  the Guition PCB, used for factory reset.
- **Buzzer, battery, photoresistor** — same exclusions as v0.x; desk-side
  USB-C powered device.
