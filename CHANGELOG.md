# Changelog

All notable, behaviour-affecting changes land here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the project
uses lightweight semantic-ish versioning (bumped on any user-visible
change, not on every commit).

## v0.9.1 — 2026-06

Hardening + performance pass from a full-project audit. No new features —
correctness, safety and internal cleanup. Like every release it lands
automatically over the air on the next boot / daily reboot.

### Fixed

- **UI thread-safety.** The header connectivity readout was mutated directly
  from the network task (core 0) while the UI task (core 1) was rendering —
  LVGL is not reentrant, so a redraw could race a label write. The net task now
  parks the state and the UI task applies it (`header_apply()`).
- **OTA integrity.** The boot updater verified the downloaded image only with
  Update's ESP-magic-byte check. The release manifest now carries an `md5`
  (alongside the existing `sha256`) and the device binds it via
  `Update.setMD5()`, so a corrupted or tampered binary is rejected at flash
  time instead of half-writing an app slot.
- **No reboot-loop on low memory.** `LV_USE_ASSERT_NULL` / `LV_USE_ASSERT_MALLOC`
  are gated to dev builds; release firmware no longer panics (and silently
  reboot-loops on a headless device) on a transient allocation failure.

### Changed

- **Lighter UI refresh.** The Printers and History screens reuse a fixed widget
  pool instead of destroying and rebuilding every row each tick, and the
  per-frame `String` allocations across the dashboard / settings / AMS screens
  are replaced with stack `snprintf`. Much less heap churn over long uptimes.
- **Less redundant polling.** While the WebSocket is pushing live status, REST
  polling drops to a single focused-printer safety-net request instead of
  re-fetching every printer.
- **Reproducible builds.** LovyanGFX and LVGL are pinned to exact versions
  rather than caret ranges.

### Docs

- `docs/assembly.md` rewritten for the integrated Guition board (it still
  described the retired v0.x ILI9488 + jumpers + WS2812 build).
- Factory-reset notes corrected: the reset wipes *all* persisted settings
  (Wi-Fi + Bambuddy creds, timezone, reboot hour, brightness, language), not
  just the network credentials.
- The web installer hides its Install button when the manifest still reports
  the `0.0.0-dev` placeholder (a Pages deploy that ran before release CI).

## v0.9.0 — 2026-06

Multilingual UI. The interface language is now picked in the captive portal,
in five languages.

### Added

- **UI language selection (EN / ES / FR / PT / DE).** A `Language` field in the
  captive portal sets the interface language (NVS key `lang`); it's applied at
  boot before the screens are built. New `i18n` module (`firmware/src/ui/i18n.*`)
  holds the `Str` enum + the five-language table and `tr()`; every user-facing
  string goes through it.
- **Extended fonts.** The built-in Montserrat fonts (ASCII only) are replaced by
  `bb_font_*` — Montserrat regenerated (`lv_font_conv`) with the Latin-1 range so
  accented characters (é, ñ, ü, ç, ã…) render, plus the FontAwesome symbol glyphs
  the UI uses. Sources in `firmware/src/ui/fonts/`.

### Notes

- Latin-script languages only: Chinese / Hindi / Arabic are out of scope (they'd
  need multi-MB CJK/Devanagari fonts, and Arabic needs RTL). French is included
  as requested.
- Brand / acronym terms (Bamboard, Bambuddy, AMS, HMS), units and the Bambu
  speed-mode names (Silent / Standard / Sport / Ludicrous) are kept verbatim.
- The first-boot setup portal itself stays in English (it's where you pick the
  language). Existing devices default to English until the portal is re-run.

## v0.8.0 — 2026-06

Timezone + reboot hour are now set in the captive portal, so one firmware build
works for users in any timezone — no more editing `config.h` and re-flashing.

### Added

- **Timezone in the captive portal.** A `tz` field takes a POSIX TZ string
  (the portal shows ready-to-paste examples for Paris / London / US East /
  UTC). Stored in NVS and applied to `configTzTime()` at boot, so the daily
  reboot fires at the *user's* local midnight.
- **Reboot hour in the captive portal.** A `reboot_h` field sets the daily
  reboot hour (`0`–`23`); leave it blank to disable the daily reboot entirely.

### Changed

- `schedule::TZ` / `DAILY_REBOOT_HOUR` / `DAILY_REBOOT_ENABLED` in `config.h`
  are now just the **first-boot defaults** — the NVS values written by the
  portal take precedence. Existing devices keep their current behaviour
  (default Europe/Paris, 00:00) until the portal is re-run.

## v0.7.0 — 2026-06

Unattended overnight updates. The device reboots itself once a day so the
boot-time GitHub OTA check applies new firmware without anyone touching it.

### Added

- **Daily auto-reboot.** The device syncs the clock over SNTP and reboots at
  local midnight (`schedule::DAILY_REBOOT_HOUR`, default `0`). On the reboot
  the existing boot-time OTA check pulls and flashes any newer release, so
  updates land overnight — no on-device prompt, no power-cycle. Implemented in
  `main.cpp` (SNTP via `configTzTime()` after Wi-Fi, reboot window checked in
  `net_task`); tunables live in the new `schedule` namespace in `config.h`.

### Notes

- The boot-time OTA stays **automatic** (no prompt on the device) — the daily
  reboot is simply what makes that check run regularly on an idle device.
- Set the timezone with `schedule::TZ` (POSIX TZ string, default Europe/Paris).
  The reboot won't fire until SNTP has synced the clock, and a guard stops the
  midnight reboot from looping. Set `DAILY_REBOOT_ENABLED = false` to disable.

## v0.6.0 — 2026-06

Browser-based first install. Flashing a fresh device now happens entirely on a
web page — no PlatformIO, no CLI.

### Added

- **Web installer** (`web/`, published to GitHub Pages at
  <https://clabeuhtegrite.github.io/Bamboard/>). An
  [ESP Web Tools](https://esphome.github.io/esp-web-tools/) page flashes the
  full firmware over Web Serial straight from the browser. It's rebuilt and
  redeployed on every release, so it always installs the latest firmware.
- **Factory image asset** — `release.yml` now also produces
  `bamboard-factory.bin` (bootloader + partition table + boot_app0 + app,
  merged with `esptool merge_bin`) and attaches it to the release. The web
  installer serves it **same-origin** (GitHub release assets don't send CORS
  headers, so the browser can't fetch the bin cross-origin); it's also the
  file to flash by hand with esptool at `0x0`.

### Removed

- **The CLI flash scripts** (`scripts/flash-*.{bat,command,sh}`, `flash.py`,
  `_common.py`). First install is the web page now; advanced users can still
  flash `bamboard-factory.bin` with esptool (see [docs/flashing.md](docs/flashing.md)).

### Notes

- **Browser support:** Web Serial is **Chromium-desktop only** (Chrome / Edge /
  Opera). Firefox, Safari and mobile browsers can't flash and fall back to
  esptool.
- OTA is unchanged: the device still pulls the app `firmware.bin` from the
  latest release at boot.

## v0.5.0 — 2026-05

Self-updating firmware. The device now pulls new firmware straight from
this repo's GitHub Releases instead of a PC pushing it over the LAN. No
hardware change.

### Added

- **Over-the-air updates from GitHub Releases.** On every boot the device
  fetches `releases/latest/download/manifest.json`, compares its version
  against the running build, and — if a newer release exists — downloads
  the `firmware.bin` asset and flashes it (full-screen progress overlay)
  before normal operation. Offline / GitHub-down / already-current all
  fall through so the device still boots. Implemented in
  `firmware/src/net/github_ota.{h,cpp}`; the boot-time call lives in
  `main.cpp::run_boot_update()`.
- **CI release pipeline** — `.github/workflows/release.yml` builds the
  firmware and publishes a GitHub Release carrying `firmware.bin` +
  `manifest.json` on every pushed `v*` tag. The version is baked into the
  build from the tag (`-DBAMBOARD_FW_VERSION=…`).
- **Build version in the UI** — the Settings screen now shows
  `Firmware <version>` so you can confirm an OTA actually took.
- **Dev-build guard** — locally compiled builds (no tag) report version
  `0.0.0-dev` and skip the boot-time auto-update, so a USB-flashed
  work-in-progress isn't immediately overwritten by the latest release.
  `-DBAMBOARD_OTA_AUTOCHECK=0` disables the check explicitly too.

### Removed

- **The desktop simulator (`sim/`).** UI iteration now happens on real
  hardware. Removes the CMake + SDL2 harness, its stubs, and the
  `BAMBOARD_SIM_LIVE` live-backend path.
- **LAN OTA push.** Dropped `ArduinoOTA` from the firmware along with the
  `scripts/update-*` launchers and `scripts/ota.py` (mDNS discovery /
  espota). The `OTA_PASSWORD_OVERRIDE` / `BAMBOARD_OTA_PASSWORD` knobs and
  the `ota::HOSTNAME` / `ota::PASSWORD` config go with them. First-time
  install is still the USB `scripts/flash-*` path.

### Fixed

- **The firmware now actually compiles for the Guition JC4827W543.** The
  first CI build surfaced three issues that had gone unnoticed without a build
  pipeline: the display HAL used `lgfx::Bus_RGB` / `lgfx::Panel_RGB` without
  including the ESP32-S3 RGB headers (`<LovyanGFX.hpp>` doesn't pull them in);
  the `PrinterState` alias `PS` collided with the Xtensa `PS` register macro
  from `<Arduino.h>`; and the linked image (~1.4 MB) overflowed `default.csv`'s
  1.25 MB OTA slots, so the partition table moved to `min_spiffs.csv` (still
  two OTA slots, ~1.9 MB each — Bamboard keeps all config in NVS, not SPIFFS).
- **CI build workflow** (`.github/workflows/ci.yml`) compiles the firmware on
  every pull request and push to `main`, so a build break is caught before a
  release tag is cut rather than after.

### Migration notes

- **Repo**: to start shipping OTA, tag a commit `vX.Y.Z` and push the tag;
  CI builds and publishes the release. Flash that build once over USB, and
  every release after it lands over the air.
- **Tags**: use plain `vMAJOR.MINOR.PATCH` — the numeric core is what the
  device compares.

## v0.4.0 — 2026-05

Polish pass on the v0.3 touch UI plus a slimmer second-gen case. No
hardware changes — same Guition JC4827W543 BOM, same one-click flash
tooling. v0.3 owners get a re-flash + a one-night reprint of the case.

### Added

- **Segmented speed chip on Live** — the v0.3 "Speed: Standard → Sport"
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
- **Sim live mode** — the desktop simulator can now talk to a real
  Bambuddy instead of the canned fixtures. Configure with
  `cmake -DBAMBOARD_SIM_LIVE=ON`, set `BAMBUDDY_URL` + `BAMBUDDY_KEY`
  in the environment, and every UI tap fires a real POST against the
  server. A 2-second background poller refreshes the cached state.
  HTTPS + WebSocket push are out of scope; plain HTTP REST polling is
  enough for UI iteration. See `sim/README.md` for details.

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
- **Case rebuilt for compactness** — `case/bamboard.scad` v0.4:
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

## v0.3.0 — 2026-05

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

If you've got the v0.x build running and want to switch to v0.3:

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
