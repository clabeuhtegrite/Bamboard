# Changelog

All notable, behaviour-affecting changes land here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the project
uses lightweight semantic-ish versioning (bumped on any user-visible
change, not on every commit).

## v1.1.0 — 2026-05

Polish pass on the v1.0 touch UI plus a slimmer second-gen case. No
hardware changes — same Guition JC4827W543 BOM, same one-click flash
tooling. v1.0 owners get a re-flash + a one-night reprint of the case.

### Added

- **Segmented speed chip on Live** — the v1.0 "Speed: Standard → Sport"
  button (cycle-on-tap) is replaced by a 4-segment Silent / Standard /
  Sport / Ludicrous chip. Active segment highlighted in accent; tap any
  segment to set that speed directly. No more cycling through three to
  reach Ludicrous.
- **AMS prev / next side buttons** — chevrons now sit flanking the unit
  label as proper 36 × 32 tap targets (was a tiny clustered pair tucked
  under the title). Visible only when the focused printer has more than
  one chained AMS / AMS-HT unit.
- **AMS drying control** — one-tap `Dry` pill on the AMS-HT status
  strip starts a 60 min @ 55 °C cycle (Bambu's PLA / PETG default).
  When a cycle is active the pill turns red and reads `Stop`. Plumbed
  through `bambuddy::Client::start_ams_drying()` /
  `stop_ams_drying()`. Bambuddy doesn't ship the route upstream yet, so
  the POSTs land on a placeholder path; we'll re-point them once the
  Bambuddy dryer endpoint lands.
- **Brightness 1–5** in Settings — segmented selector, identical visual
  language to the speed chip. Persisted to NVS as `bl_level`, applied
  at boot, and used as the auto-dim wake target instead of the
  hard-coded full PWM. Default level 3 (≈ 50 % PWM).
- **Subtle accent indicator** above the active tab — 3 px strip in
  C_ACCENT that fades in on tap. The selected screen is obvious from
  across the room without reading the labels.

### Changed

- **Modern theme tokens** — every screen now reads radius / colour /
  button-height from `ui::R_PANEL`, `ui::R_BUTTON`, `ui::R_PILL`,
  `ui::H_BTN` in `config.h`. Tweak one constant and the whole UI moves
  together. New `C_PANEL_LINE` hairline colour for borders,
  `C_ACCENT_DARK` for pressed states.
- **Press state** on every button shares one `s_btn_pressed` style — a
  consistent darken + 1 px scale-down on tap. Was previously absent on
  most buttons.
- **Header + tab bar hairlines** — 1 px C_PANEL_LINE underline /
  overline so the chrome reads as separated layers without a heavy
  divider rule.
- **Printers rows** picked up the new panel radius (12 px) + press
  state, focused row gets a 2 px accent border (was 2 px without
  state).
- **Action button shape** moved to pill (R_PILL = 18 px) for primary
  CTAs to differentiate them from neutral chips.
- **Case rebuilt for compactness** — `case/bamboard.scad` v1.1:
  - PCB pocket clearance dropped 6 mm → 0.6 mm. Overall case 8 mm
    narrower, 6 mm shorter.
  - Walls trimmed 2.4 mm → 1.8 mm; top 2.0 mm → 1.6 mm.
  - Bezel around the active area dropped ~6 mm → ~3 mm.
  - Chamfered front edge (0.6 mm 45°) so it reads as one solid piece.
  - Vent slots rotated to vertical and grouped over the ESP module.
  - **Integrated 15° desk-stand tab** on the back shell — no
    accessory print needed.
  - Mating screws **M3 × 6 mm** (was 8 mm) to suit the thinner walls.
  - Total filament for both shells: ~32 g (was ~50 g).

### Removed

- The "cycle-on-tap" speed button. The new segmented chip exposes all
  four modes simultaneously.

### Migration notes

- **Firmware**: re-flash via `scripts/flash-*` or `scripts/update-*`.
  Your captive-portal Wi-Fi + Bambuddy creds carry over; brightness
  starts at the default level 3 on first boot after the upgrade.
- **Case**: print the new front + back shells from `case/bamboard.scad`
  and swap. The board itself drops in unchanged; the four corner
  screws change from M3 × 8 mm to M3 × 6 mm.

## v1.0.0 — 2026-05

Major architectural pivot: the standalone ESP32-S3 DevKitC + ILI9488
SPI + 3 button modules + WS2812 breakout build is replaced by a
single all-in-one board (Guition JC4827W543: ESP32-S3-WROOM-1 with
8 MB PSRAM, 4.3" IPS 480 × 272 RGB-parallel panel, GT911 capacitive
touch on the same PCB). Zero soldering, zero wiring, BOM down to
~20 €.

### Added

- **Touch UI** end-to-end. Bottom tab bar with 5 tabs (Live / AMS /
  Printers / History / Settings). Tap a tab to switch; swipe left /
  right inside the body to move to the next / previous tab.
- **Inline contextual actions on Live** — `Speed: <current> →
  <next>` while printing, `Clear plate` when finished, `Clear HMS`
  when an error is active. No more long-press / double-click.
- **Tap-a-row on Printers** focuses that printer and auto-navigates
  to Live.
- **Two-tap Factory reset** button on Settings — first tap arms it
  for 3 s, second tap calls `factory_reset()` which wipes NVS and
  reboots into the captive portal.
- **HMS overlay tap-to-dismiss** instead of swallowing the next
  button press.
- **AMS unit cycling** via on-screen ◀ / ▶ buttons in the status
  strip when multiple AMS units are present.
- **Desktop LVGL simulator** under `sim/` — CMake + SDL2 harness
  that runs the same `firmware/src/ui/{screens,ui}.cpp` sources at
  the real 480 × 272 resolution. Mouse drives the touch input
  device. Validates layout + navigation + canned-snapshot rendering
  without hardware.

### Changed

- `platformio.ini` swaps `bodmer/TFT_eSPI` + `fastled/FastLED` for
  `lovyan03/LovyanGFX`. Same `esp32-s3-devkitc-1` board target
  (the WROOM-1 module is identical), but the partition layout
  drops to 4 MB to match the Guition's flash.
- `firmware/src/hw/display.{h,cpp}` rewritten on top of LovyanGFX
  with a JC4827W543**C** pin map at the top of the file (RGB bus +
  GT911 + backlight PWM). All other firmware sources stay
  identical except for losing the `selected_index_` / button-event
  state that's no longer relevant.
- `firmware/src/ui/screens.cpp` redesigned for the new 480 × 272
  body area (192 px tall between the 36 px header and the 44 px
  tab bar). All five screens recompacted.
- Wi-Fi captive-portal factory-reset trigger moves from "hold PREV
  at boot" to "hold BOOT at boot" (GPIO 0, the side-mounted
  tactile switch on the Guition PCB).
- `case/bamboard.scad` rewritten for the JC4827W543 PCB: two
  shells (front with display cut-out + board pocket + optional M3
  standoffs, back with USB-C / BOOT / RST / SD cut-outs + vents).
  ~20 % less filament than the v0.x case.
- `hardware/{bom,wiring}.md` collapsed to a single board + a USB
  cable. wiring.md is now a one-paragraph "plug USB-C in" note.

### Removed

- `firmware/src/hw/buttons.{h,cpp}` — no more external buttons.
- `firmware/src/hw/led.{h,cpp}` — no more WS2812 module. The HMS
  alert pulses run as a full-screen red overlay on the panel.
- Actions modal (`screens::build_actions / actions_open / …`) —
  every interaction is a direct on-screen tap now.
- `Manager::handle_input()` — LVGL's pointer indev (fed by the
  GT911) drives the UI via event callbacks, no manual event pump.
- `User_Setup.h` (TFT_eSPI's config) and `lib_deps` for TFT_eSPI +
  FastLED.

### Migration notes

If you've got the v0.x build running and want to switch to v1.0:

1. Buy a Guition JC4827W543 (see `hardware/bom.md` for AliExpress
   search links). The DevKitC + ILI9488 SPI + buttons + LED parts
   you already have don't carry over — sell them or use them in a
   different project.
2. Print the new case (`case/bamboard.scad` part = "front" / "back").
3. Flash via `scripts/flash-windows.bat` / `flash-mac.command` /
   `flash-linux.sh` as before. The OTA update path still works on
   the new board.
4. The captive-portal AP name + password and the Bambuddy URL / API
   key format are unchanged, so the same first-boot setup applies.

## v0.2.0 — 2026-05

The Bambuddy-side features that were claimed in v0.1's README but
never actually wired, plus the install/update tooling that was missing
to make day-to-day use painless.

### Added

- **AMS screen** between Live and Printers. Per-slot colour / filament
  type / remaining %, plus AMS humidity, temperature and active drying
  countdown. Long-press OK cycles through chained AMS / AMS-HT units.
  The `bambuddy_client` data model gained `AmsSlot` / `AmsUnit` structs
  and `Printer::ams_exists` / `ams_count` / `ams[]`.
- **Per-printer actions modal** with Clear HMS / Clear plate / Cancel.
  Opens via long-press OK on the Printers screen.
- **Contextual quick-actions popup on Live**, triggered by double-click
  OK. Items depend on the printer state:
  - while printing → "Speed: \<current\> → \<next\>"
  - when finished → "Clear plate"
  - always → "Clear HMS"
- **Double-click button event** added to `hw::Buttons`. `Press` keeps
  firing immediately on release; `DoublePress` fires additionally when
  a second short release lands within 350 ms.
- **WebSocket push** via Bambuddy's `/ws`. New `bambuddy::WsClient`
  parses the host out of the captive-portal URL, supports plain + SSL,
  sends app-level `{"type":"ping"}` every 25 s, dispatches
  `printer_status` frames into the shared `Client::apply_status_payload`.
- **Adaptive polling**: REST status polling stays at 2 s when WS is
  down, falls back to 30 s as a safety net when WS is connected.
- **Full-screen HMS flash** overlay. Pulses between two reds at ~1.4 Hz,
  visible 5 s every 30 s while the error persists. Any button dismisses
  the current pulse; the next cooldown re-arms automatically so the
  alert can't be silenced indefinitely.
- **ArduinoOTA** wired in `setup()`. Default password `bamboard`,
  overridable at build time via `-DOTA_PASSWORD_OVERRIDE='"..."'`.
  Full-screen progress overlay on the device during upload, friendly
  error messages on `OTA_AUTH_ERROR` / `OTA_CONNECT_ERROR` / etc.
- **One-click install scripts** (`scripts/flash-{windows.bat,
  mac.command,linux.sh}`) — auto-detects the ESP32 serial port via
  VID:PID scoring, runs `pio run -t upload`. Optional `--erase`,
  `--monitor`, `--port`, `-y`, `--build-only`.
- **One-click update scripts** (`scripts/update-{windows.bat,
  mac.command,linux.sh}`) — mDNS discovers `bamboard.local`, caches
  the IP on success, calls `pio run -t upload --upload-port ...`.
  Optional `--reset`, `--host`, `--password`, `--build-only`.
- Per-printer hover behaviour on the Printers screen long-OK: opens the
  actions modal for the *hovered* row, not for the dashboard printer.

### Changed

- `fetch_printer_status` was refactored into a thin HTTP fetch + a
  shared `apply_status_payload(int, JsonVariantConst)` that both the
  REST poller and the WebSocket dispatch path call. No behaviour
  change — both write the same fields under the same mutex.
- `Printer::speed_level` is now read from `/status` so the long-press
  OK speed cycle uses the live value instead of a stale local counter
  (which drifted whenever Bambu Studio or the popup changed the mode).
- Modal layout now supports up to 5 dynamic items (was a fixed 3). The
  long-OK call from Printers screen explicitly passes its item array;
  Live uses `actions_open_for_live()` which picks items by state.
- Existing nav-dot mockups (`docs/screenshots/*.svg`) bumped from 4 to
  5 dots to reflect the new AMS screen position.
- `Manager::handle_input()` consumes button events while an overlay is
  active: OTA overlay → drop the event entirely; HMS flash → dismiss
  the overlay and consume; actions modal → routed to modal handlers.

### Documentation

- `README.md` features list reorganised into thematic subsections
  (Monitoring / Alerts / Controls / Connectivity / Install & updates).
- `docs/flashing.md` rewritten around the launcher scripts with a
  manual fallback for the raw `pio` commands.
- `scripts/README.md` added — index of every file in `scripts/` and
  the conventions to follow when adding a new launcher.
- `.gitattributes` added to pin LF on `.sh` / `.command` / `.py` so a
  clone on macOS / Linux gets working shebang lines regardless of the
  contributor's `autocrlf` setting.

## v0.1.0 — initial release

First public release. Live dashboard, multi-printer, history & stats,
print-speed control, HMS surfacing (label + LED only), Wi-Fi captive
portal, status LED, auto-dim. ArduinoOTA was advertised in the README
but not wired in code — fixed in v0.2.
