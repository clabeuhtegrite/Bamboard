# Bill of materials

Indicative prices in EUR, sourced mostly from AliExpress / Amazon at the
time of writing. Local electronics stores (TME, Mouser, Reichelt) will of
course be more expensive but will deliver in days, not weeks.

| # | Part                                                  | Qty | ~Price | Notes |
|---|-------------------------------------------------------|-----|--------|-------|
| 1 | ESP32-S3 DevKitC-1 (N8R2 or N8R8, with PSRAM)         | 1   | 6 €    | Must be an S3 variant with PSRAM. The "N8R2" suffix means 8 MB flash + 2 MB PSRAM, which is plenty. |
| 2 | 4.0" TFT LCD ILI9488, SPI interface, 480×320          | 1   | 12 €   | Make sure to buy the **SPI** version (4-wire). The parallel 8/16-bit "MCUFRIEND" version will not work without rewiring. |
| 3 | Tactile button **module** (3-pin, pre-soldered headers) | 3   | 1.50 € | The bare 6×6 mm Omron-style switches that ship in most "Arduino kits" need a soldering iron. We want the **module** version: one button mounted on a tiny PCB with `S` / `+` / `−` (or `SIG` / `VCC` / `GND`) male pin headers already in place. Often sold as "KY-004" or "tactile button module". |
| 4 | WS2812B "1-bit module" **with pre-soldered headers**   | 1   | 1.50 € | Sold as "WS2812 RGB LED module". Look at the listing photo: a usable one shows 3 male pin headers already soldered into the `VCC` / `GND` / `DIN` holes. The cheaper SKU with empty plated holes is *not* plug-and-play — same SMD LED but you'd have to solder the headers yourself. |
| 5 | Female-female Dupont jumper wires 20 cm               | 12  | 1 €    | Or one ribbon cable. Plug-in only, no soldering required. |
| 6 | USB-C cable (data, not power-only)                    | 1   | 2 €    | For flashing and powering the device. |
| 7 | M2.5 × 8 mm self-tapping screws                       | 4   | 0.50 € | To clip the case halves together (or M3 if you prefer; the SCAD file is parametric). |
| 8 | M2 × 6 mm screws + nuts                               | 4   | 0.50 € | To mount the display to the front shell. |
| 9 | PLA / PETG filament                                   | ~80 g | 1.20 € | For the printed enclosure. |

**Total: ~26–30 €** depending on sourcing.

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

### 3. Tactile button module (3-pin, pre-soldered headers)

- [Search: "KY-004 tactile button module Arduino"](https://www.aliexpress.com/wholesale?SearchText=KY-004+tactile+button+module+Arduino)
- [Search: "tactile push button module 3 pin"](https://www.aliexpress.com/wholesale?SearchText=tactile+push+button+module+3+pin)

The thing to look for on the listing photo: a small PCB (~12 × 12 mm)
with the 6×6 mm tact switch already soldered in the middle and **three
visible male pin headers** sticking out the bottom, usually labelled
`S` / `+` / `−`. Female-female Dupont jumpers plug directly onto those
headers. Often sold in 5-packs or as part of a "37-in-1 sensor kit" —
the kit usually works out cheaper than buying 3 modules separately and
gives you spares.

Avoid: the bare loose tactile switches (no PCB, just 4 metal legs) and
the "Arduino keypad 4×4" matrix modules — neither plugs in cleanly.

### 4. WS2812B 1-bit module (with pre-soldered headers)

- [Search: "WS2812 RGB LED module with header pins"](https://www.aliexpress.com/wholesale?SearchText=WS2812+RGB+LED+module+with+header+pins)
- [Search: "WS2812B 1 bit module pre-soldered headers"](https://www.aliexpress.com/wholesale?SearchText=WS2812B+1+bit+module+pre-soldered+headers)

The thing to look for on the listing photo: the same tiny RGB LED on a
postage-stamp PCB, but with **3 male pin headers already soldered** in
the `VCC` / `GND` / `DIN` holes. Two SKUs of the *exact same* board
ship side-by-side — one with the headers in place, one with just
empty solder holes. The empty-holes one is cheaper but defeats the
no-soldering goal, so zoom on the product photo to confirm before
ordering.

Filtering tip: add the word `header` or `pin` to the search if the
results lean toward bare-PCB listings.

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
- **WS2812 module with pre-soldered headers, not raw LED** — you wanted
  "no soldering". The module includes the 100 nF cap baked in, and the
  *with-headers* SKU lets you plug Dupont jumpers straight onto its 3
  pins. The bare-PCB SKU of the same module costs ~0.50 € less but
  needs an iron — pick carefully.
- **Button modules, not bare tactile switches** — same reason: the
  6×6 mm tact switches you find in every starter kit have 4 bare metal
  legs. Wiring them to anything but a perfboard means soldering. The
  3-pin "KY-004"-style modules carry the same switch on a small PCB
  with `S` / `+` / `−` headers already in place; Dupont jumpers plug
  directly on.
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
