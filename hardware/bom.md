# Bill of materials

Indicative prices in EUR, sourced mostly from AliExpress / Amazon at the
time of writing. Local electronics stores (TME, Mouser, Reichelt) will of
course be more expensive but will deliver in days, not weeks.

| # | Part                                                  | Qty | ~Price | Notes |
|---|-------------------------------------------------------|-----|--------|-------|
| 1 | ESP32-S3 DevKitC-1 (N8R2 or N8R8, with PSRAM)         | 1   | 6 €    | Must be an S3 variant with PSRAM. The "N8R2" suffix means 8 MB flash + 2 MB PSRAM, which is plenty. |
| 2 | 4.0" TFT LCD ILI9488, SPI interface, 480×320          | 1   | 12 €   | Make sure to buy the **SPI** version (4-wire). The parallel 8/16-bit "MCUFRIEND" version will not work without rewiring. |
| 3 | 6×6 mm tactile buttons, through-hole, 5 mm stem       | 3   | 0.50 € | Standard "Omron-style" pushbuttons. Choose 7 mm stems if you want a flusher fit. |
| 4 | Pre-soldered WS2812B "1-bit module" (3 pins)          | 1   | 1 €    | Sold as "WS2812 RGB LED module" or "Adafruit NeoPixel breakout". The cheap AliExpress 3-pin module with VCC/GND/DIN screwed-on headers is fine — no soldering. |
| 5 | Female-female Dupont jumper wires 20 cm               | 12  | 1 €    | Or one ribbon cable. Plug-in only, no soldering required. |
| 6 | USB-C cable (data, not power-only)                    | 1   | 2 €    | For flashing and powering the device. |
| 7 | M2.5 × 8 mm self-tapping screws                       | 4   | 0.50 € | To clip the case halves together (or M3 if you prefer; the SCAD file is parametric). |
| 8 | M2 × 6 mm screws + nuts                               | 4   | 0.50 € | To mount the display to the front shell. |
| 9 | PLA / PETG filament                                   | ~80 g | 1.20 € | For the printed enclosure. |

**Total: ~25–28 €** depending on sourcing.

## Where to buy (AliExpress)

Two search links per item — they're search queries, not product URLs,
so they don't rot when a specific listing disappears. Pick the seller
with the best feedback / shipping window when you order. Every part
below stays compatible with the **no-soldering** assembly: the modules
ship with pre-soldered headers and the kit screws together with
plug-in Dupont jumpers.

### 1. ESP32-S3 DevKitC-1 (N8R2 or N8R8)

- [Search: "ESP32-S3 DevKitC-1 N8R2"](https://www.aliexpress.com/wholesale?SearchText=ESP32-S3+DevKitC-1+N8R2)
- [Search: "ESP32-S3 DevKitC-1 N8R8 PSRAM"](https://www.aliexpress.com/wholesale?SearchText=ESP32-S3+DevKitC-1+N8R8+PSRAM)

In the listing, check the back of the board photo for the metal cans
labelled `ESP32-S3` and `PSRAM` (or `N8R2` / `N8R8` on a sticker). Avoid
the bare `ESP32-S3-WROOM-1` module without USB headers — you'd have to
solder your own USB connector.

### 2. 4.0" TFT LCD ILI9488 SPI (480×320)

- [Search: "ILI9488 4 inch SPI 480x320"](https://www.aliexpress.com/wholesale?SearchText=ILI9488+4+inch+SPI+480x320)
- [Search: "4.0 inch TFT SPI display ILI9488"](https://www.aliexpress.com/wholesale?SearchText=4.0+inch+TFT+SPI+display+ILI9488)

Filter on the **SPI** wording in the title or photo — the same panel
also ships in a 16-bit parallel "MCUFRIEND" version with a chunky
0.1″ header at the bottom; that one needs ~20 wires and won't fit our
layout. The SPI variant uses a single 14-pin (or sometimes 18-pin)
0.1″ header on the side.

### 3. 6×6 mm tactile buttons (through-hole, 5 mm stem)

- [Search: "tactile button 6x6 5mm through hole"](https://www.aliexpress.com/wholesale?SearchText=tactile+button+6x6+5mm+through+hole)
- [Search: "tact switch 6x6mm DIP 4 pin"](https://www.aliexpress.com/wholesale?SearchText=tact+switch+6x6mm+DIP+4+pin)

Buttons in this format are usually sold in 50- or 100-packs for under
2 €. We only need 3, so the rest is spares. Pick the **5 mm stem** for
a flush fit with the case lid; 7 mm sticks out a couple millimetres.

### 4. WS2812B 1-bit module (pre-soldered, 3 pins)

- [Search: "WS2812 RGB LED module 1 bit"](https://www.aliexpress.com/wholesale?SearchText=WS2812+RGB+LED+module+1+bit)
- [Search: "WS2812B breakout 3 pin VCC GND DIN"](https://www.aliexpress.com/wholesale?SearchText=WS2812B+breakout+3+pin+VCC+GND+DIN)

Look for the model that has the LED already soldered to a tiny breakout
PCB with VCC / GND / DIN pin headers — that's the no-solder one. The
bare WS2812B SMD chip on its own would need a hot air station.

### 5. Female-female Dupont jumper wires (20 cm, 40 pin ribbon)

- [Search: "Dupont jumper wire female female 20cm 40pin"](https://www.aliexpress.com/wholesale?SearchText=Dupont+jumper+wire+female+female+20cm+40pin)
- [Search: "40 pin female female ribbon cable 20cm"](https://www.aliexpress.com/wholesale?SearchText=40+pin+female+female+ribbon+cable+20cm)

You only need 12 lines, but the cables ship as a 40-pin rainbow ribbon
you split by hand. 20 cm is the sweet spot for routing inside the
case; the 10 cm version is too tight, the 30 cm one bunches up.

### 6. USB-C cable (data, not power-only)

- [Search: "USB C cable data sync 1m"](https://www.aliexpress.com/wholesale?SearchText=USB+C+cable+data+sync+1m)
- [Search: "USB-C 2.0 data cable 1m"](https://www.aliexpress.com/wholesale?SearchText=USB-C+2.0+data+cable+1m)

The thin white cables bundled with phone chargers are often
charge-only — they'd power the device but PlatformIO wouldn't see it
during the first flash. Look for "data" explicitly in the listing
title, or for "USB 2.0" or "480 Mbps".

### 7. M2.5 × 8 mm self-tapping screws

- [Search: "M2.5 8mm self tapping screw black"](https://www.aliexpress.com/wholesale?SearchText=M2.5+8mm+self+tapping+screw+black)
- [Search: "M2.5 PA self-tap screw 8mm assortment"](https://www.aliexpress.com/wholesale?SearchText=M2.5+PA+self-tap+screw+8mm+assortment)

Assortment kits at ~3 € give you M2 to M5 in 4 to 16 mm lengths, which
covers both this row and row #8. If you'd rather buy exactly what's
needed, the dedicated single-spec packs run ~1 €.

### 8. M2 × 6 mm screws + nuts

- [Search: "M2 6mm screw nut set"](https://www.aliexpress.com/wholesale?SearchText=M2+6mm+screw+nut+set)
- [Search: "M2 machine screw 6mm hex nut kit"](https://www.aliexpress.com/wholesale?SearchText=M2+machine+screw+6mm+hex+nut+kit)

Same logic as #7 — the multi-pack assortments include both screws and
nuts in the right metric pitches.

### 9. PLA / PETG filament

- [Search: "PLA filament 1.75mm 1kg"](https://www.aliexpress.com/wholesale?SearchText=PLA+filament+1.75mm+1kg)
- [Search: "PETG filament 1.75mm 1kg"](https://www.aliexpress.com/wholesale?SearchText=PETG+filament+1.75mm+1kg)

You probably already own a partial spool. ~80 g covers the case.
PETG handles desk heat (sun, summer) better than PLA if the device
sits in a windowsill.

## Why these parts?

- **ESP32-S3, not C3** — we ruled out the C3 because driving a 4″ display
  while keeping LVGL animations smooth needs the S3's dual-core +
  PSRAM. The C3 would have worked at 1.9″ but felt too cramped for the
  desired UI.
- **ILI9488 SPI** — the only widely-available 4″ panel that talks SPI.
  Cheaper 3.5″ panels (ILI9486 / ST7796) work too if you tweak
  `User_Setup.h`.
- **WS2812 module, not raw LED** — you wanted "no soldering". The module
  comes with screw or pin headers and includes the 100 nF cap + level
  shifter footprint baked in.
- **3 buttons** — minimum for a navigable carousel UI. Short-press to
  switch screen, long-press for context actions (see the firmware comments).

## Optional extras

| Part                                              | Why                                   |
|---------------------------------------------------|---------------------------------------|
| 0.1″ JST-XH 4-pin connector + crimped pigtail     | If you want to factor out the display ribbon cleanly. |
| Right-angle USB-C breakout                        | If your desk space favours a side cable exit. |
| 4× M3 brass heat-set inserts                      | Sturdier than self-tapping into plastic; needs a soldering iron though, so I left them off the default BOM. |

## What we deliberately left out

- **Buzzer** — by your request.
- **Photoresistor for auto-dim** — replaced by timer-based dim in firmware (`DIM_AFTER_MS`).
- **Battery / charging** — the device is desk-side, USB-C powered.
