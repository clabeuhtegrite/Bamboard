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
