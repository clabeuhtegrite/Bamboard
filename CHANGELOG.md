# Changelog

All notable, behaviour-affecting changes land here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the project
uses lightweight semantic-ish versioning (bumped on any user-visible
change, not on every commit).

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
