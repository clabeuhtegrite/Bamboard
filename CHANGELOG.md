# Changelog

All notable, behaviour-affecting changes land here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the project
uses lightweight semantic-ish versioning (bumped on any user-visible
change, not on every commit).

## v0.19.0 — 2026-06

A round of new at-a-glance features.

### Added

- **Printers screen is now a farm grid.** Instead of a single-column list, the
  Printers tab shows a two-up grid of tiles — each with a progress ring (accent
  arc + % while printing, a full state-coloured ring otherwise: idle grey,
  finish green, failed red), the printer name and an at-a-glance status line
  (nozzle/bed temps + ETA while printing, model + state otherwise). Tap a tile
  to focus that printer and jump to Live, as before — a control-wall view for a
  multi-printer farm.

## v0.18.2 — 2026-06

### Fixed

- **Action pills no longer wear a teal halo.** Every inline action button
  (`make_action_btn`) layered the accent style — which bakes in a teal vertical
  gradient and a teal border — then overrode only the *fill* colour. A non-teal
  pill therefore kept the teal gradient and outline, so the amber **Stop** and
  red **Clear HMS** buttons rendered shaded and ringed in teal (an ugly blue
  glow over the amber/red). Each pill now fades to a darker shade of its *own*
  colour with a matching hairline, applied at build time and whenever the colour
  changes (e.g. Stop arming to red).

## v0.18.1 — 2026-06

### Fixed

- **A printer status that arrives before the printer is listed is no longer
  dropped.** `apply_status_payload` only updated an already-known printer, so a
  WebSocket `printer_status` push landing before the first `/printers` fetch (a
  brief boot window) was discarded. It now creates the entry on first sight,
  taking the name / model from the payload when present.

### Internal

- **Host visual-regression harness.** The simulator can render deterministic
  synthetic states — printing, a 2-unit AMS (incl. AMS-HT drying), and an HMS
  flash — via `SIM_FIXTURES_ONLY`, and a CI job byte-compares them against
  committed baselines on every PR (no network, no secrets). This also exercises
  `apply_status_payload` (the WebSocket push path's handler) on states the live
  data may never show. The shim's wall clock is pinned so time-derived UI
  renders reproducibly.

## v0.18.0 — 2026-06

A feature round from the post-audit idea list: more at-a-glance status, a
couple of new live readouts, and a parsing optimisation.

### Added

- **Print finished / failed toast.** When any printer leaves a printing state
  for Finish (✓) or Failed/Error, a toast pops over whatever screen is up — so
  a finished or failed job is noticed even from another tab. One-shot per
  transition; nothing fires for an already-finished printer at boot.
- **Live status on the Printers list.** A printing/paused row now shows its
  nozzle/bed temps and remaining ETA inline (e.g. `X1C · 220°/60° · 1h12`)
  beside the progress %, turning the Printers tab into an at-a-glance
  multi-printer view without leaving it.
- **AMS drying countdown.** The drying readout ticks down live (mm:ss in the
  final hour) instead of showing a static minutes figure — re-synced to
  Bambuddy's authoritative remaining-minutes on every poll.

### Changed

- **The camera refreshes faster while the full-screen viewer is open** (~2 s vs
  the 5 s thumbnail cadence) for a more live picture; it backs off again on close.

### Internal

- **`/status` is parsed through an ArduinoJson filter.** Bambuddy mirrors the
  full MQTT report under `/status`; the firmware reads ~25 fields of it. A
  filter whitelisting exactly those fields shrinks the PSRAM document and the
  parse time on every REST status fetch. The CI simulator renders the same real
  data unchanged, confirming nothing the UI needs was filtered out.

## v0.17.3 — 2026-06

A second full-project audit pass. No critical or major bugs surfaced — the
v0.17.2 pass had already cleared those — so this is minor robustness + cleanup,
mostly in the boot/display and OTA paths the first pass under-covered.

### Fixed

- **No bright flash on boot.** The display HAL lit the backlight to full at the
  end of `Display::begin()` — before LVGL had drawn anything and before the
  user's saved brightness was applied — so the panel briefly showed the black
  pre-render fill, then the first frame, at full brightness before dimming. The
  backlight now stays dark through init; `main.cpp` raises it to the saved level
  only after the first frame, which is what the code comment already claimed.
- **OTA: a broken release manifest is no longer reported as "offline".** A
  fetched-but-unparseable manifest returned `NoNetwork` (the `BadManifest`
  result was declared but never produced), conflating a corrupt release with a
  down link. `check_and_update` now returns `BadManifest` when the server
  answered but the body was bad. Boot still continues either way.
- **OTA: the expected MD5 is validated as 32 hex characters,** not merely 32
  characters of any kind — what the code comment already promised. A malformed
  hash still fails safe at flash time; this rejects it earlier and makes the
  guard honest.
- **The Live speed picker blocks the swipe gesture while open** (like the camera
  / HMS / OTA overlays already do), so a stray flick can't navigate out from
  under the open modal.

### Internal

- `Manager::selected_printer_id_` marked `volatile`: written on the UI task,
  read on the net task (focused-printer polling + camera). The access is a
  single aligned 32-bit scalar, so volatile (fresh reads, no caching) documents
  and enforces the cross-task contract.
- Removed the unused `display::BL_RES` constant — the PWM resolution was never
  wired into `Light_PWM` (which keeps its 8-bit default). Corrected the OTA-slot
  size note in `platformio.ini` (~1.875 MB, not ~1.9 MB).

## v0.17.2 — 2026-06

Hardening + cleanup pass from a full-project audit — correctness, safety and
accuracy only, no new features. Like every release it lands automatically over
the air on the next boot / daily reboot.

### Fixed

- **Factory-reset toast rendered a tofu box.** The `Resetting…` string (shown
  the instant before the device reboots into the captive portal) used a Unicode
  ellipsis `…` (U+2026), which isn't in the firmware's Latin-1 + FontAwesome
  font — so it drew a missing-glyph box in all five languages. Replaced with
  ASCII `...`.
- **Camera: an oversized snapshot is rejected instead of shown half-decoded.**
  `fetch_camera_jpeg` capped its PSRAM buffer at 256 KiB but still returned
  `true` when the source declared a larger frame, handing a truncated JPEG to
  the decoder. It now fails cleanly ("frame too large") when the declared
  content length exceeds the cap, and clears a rejected stream token on a retry
  401 so the next fetch starts fresh.

### Security

- **The Cloudflare Access token can no longer leak onto a clear-text link.** The
  `CF-Access-Client-Id` / `CF-Access-Client-Secret` headers were attached
  whenever a token was set, relying solely on the captive portal blanking the
  token on http. The REST client and the WebSocket handshake now also gate the
  headers on the scheme (`https://` / `wss://`) at the point they're emitted —
  defence in depth, so a token that ever lingered in NVS beside an `http://` URL
  still can't go out in clear text. No change for the normal https + CF setup.

### Internal

- Dropped the dead `s_dash_speed_lbl_cap` widget on Live — the "SPEED" caption
  was removed in v0.17.0 but the (permanently hidden) label object lingered.
- Corrected stale source comments the audit surfaced: the tab bar's "5 buttons"
  (it's 6 since the Queue tab), Settings' "4 read-only rows" (5 with the
  Bambuddy server row), the default-brightness note (level 3 ≈ 50 %, not 70 %),
  and the REST client's API-endpoint list + AMS-drying route header (now
  matching the routes the code actually calls).

## v0.17.1 — 2026-06

### Fixed

- **"Clear plate" no longer appears when Bambuddy isn't waiting for it.** The pill
  was shown for any FINISH/FAILED print, so a finished job whose plate had already
  been acknowledged (or a printer that rebooted back into that state) kept
  prompting. It now follows Bambuddy's own **`awaiting_plate_clear`** flag from
  `/status` — the authoritative "an acknowledgment is expected" signal — instead
  of inferring it from the print state.

## v0.17.0 — 2026-06

More of Bambuddy's API brought to the device: control a running print, toggle
the chamber light, read the fans, see the print queue, and check the Bambuddy
server version — all on real data the simulator renders in CI.

### Added

- **Pause / Resume / Stop a print from Live.** The action row gains **Pause**
  (flips to **Resume** while paused) and **Stop** whenever a print is running.
  Stop is **two-tap** (toast-confirmed) so a stray touch can't abort a job.
  (POST `/printers/{id}/print/{pause,resume,stop}`.)
- **Chamber-light toggle** on Live — a pill that turns the printer's chamber
  light on/off (amber when lit). POST `/printers/{id}/chamber-light?on=`.
- **Fan readout.** When no HMS error is showing, the Live status line reports the
  part-cooling / auxiliary / chamber fan speeds Bambuddy already includes in
  `/status` — no extra request.
- **Print-queue screen.** A new **Queue** tab lists the jobs Bambuddy still has
  pending, in order (job name + target printer), from GET `/queue` (refreshed
  every 15 s). The tab bar is now six tabs.
- **Bambuddy server version + uptime** on Settings, from GET `/system/info`,
  beside the device's own diagnostics.

### Notes

- The control actions (pause/resume/stop, light) need an API key with
  `printers:control`; read-only keys still get the queue, fan readout and server
  version. The print-speed button drops its small "SPEED" caption to make room
  for pause/stop (it still shows the current mode).
- **Hardware-only:** the actual MQTT effect of pause/stop/light on the printer is
  confirmed on a real device; CI validates compilation + the rendered layout
  (the sim exercises the REST paths and renders every screen, incl. the queue).
- Fan-speed values are shown as the integer Bambuddy reports; the exact scale is
  pinned down on real hardware.

## v0.16.0 — 2026-06

Configurable connection: keep talking to Bambuddy over plain HTTP on the LAN, or
reach it by domain over HTTPS — optionally through Cloudflare Access. One build
covers a local box and remote/secured access; the HTTPS + Cloudflare path used to
exist only in the host simulator.

### Added

- **HTTP / HTTPS choice in the captive portal, with optional Cloudflare Access.**
  First-boot setup (and "hold BOOT" re-provisioning) gains a **Use HTTPS** toggle
  next to the Bambuddy host field:
  - **HTTP** (the default — existing devices are unchanged) accepts only a
    **private IPv4** (`192.168.x` / `10.x` / `172.16-31.x`) or an **`*.local`**
    mDNS name, optional `:port`. The LAN-direct path; the API key travels in clear.
  - **HTTPS** accepts a **domain** or IP and **validates the server certificate**
    against a root-CA bundle baked into the firmware. Ticking it reveals two extra
    fields for an optional **Cloudflare Access service token**
    (`CF-Access-Client-Id` / `CF-Access-Client-Secret`), sent on every REST,
    camera and stream-token request **and on the WebSocket handshake** — so a
    Bambuddy published behind Cloudflare Access is reachable from the device, not
    just from the sim.

### Changed

- The REST client (including the camera snapshot + stream-token) routes `https://`
  through a `WiFiClientSecure` validated by the embedded bundle; `http://` is the
  byte-for-byte previous path. The host is validated when the portal saves, so an
  HTTP box can't be pointed at a public domain by mistake (the CF token is also
  dropped on HTTP so it can't leak onto a clear-text link).

### Internal

- The root-CA bundle is **regenerated on every CI build** from the current Mozilla
  set (`firmware/scripts/gen_ca_bundle.sh` → `curl.se/ca/cacert.pem` +
  Espressif's `gen_crt_bundle.py`) and embedded via `board_build.embed_files`, so
  the trusted roots stay fresh without committing a CA list that goes stale. The
  simulator stubs the bundle symbol + the secure-client/WS shims and keeps
  reaching the real Bambuddy over libcurl, so CI still renders every screen.

### Notes

- A Bambuddy with a **self-signed** HTTPS cert won't validate against the public
  CA bundle — use HTTP on the LAN for that. The TLS handshake, the live Cloudflare
  round-trip (REST + WebSocket) and the portal's HTTPS/CF fields are
  **hardware-only**: CI compiles + renders, the secure path is confirmed on the board.

## v0.15.3 — 2026-06

### Fixed

- **The Live camera thumbnail didn't fully fill its box.** v0.15.2's cover
  scaling zoomed about a top-left pivot, which fills correctly only when
  *up-scaling* (the full-screen viewer). The thumbnail always *down-scales* the
  frame, and there the top-left pivot left the bottom-right corner unpainted
  (a black wedge). The cover helper now zooms about the image centre and centres
  the frame, so it fills the box for both up- and down-scaling. The full-screen
  viewer (already correct) is unchanged.

## v0.15.2 — 2026-06

### Changed

- **The camera now fills the frame.** Both the full-screen viewer and the Live
  thumbnail scaled the snapshot to *fit inside* their area (contain), so a
  ~16:9 frame sat in a small box ringed by thick black bars. They now scale it
  to *fill* the area (cover): the frame is enlarged until it covers the box and
  the slight overflow is clipped — no distortion, no letterbox. The full-screen
  "Tap to dismiss" hint gained a translucent pill so it stays legible now that
  it sits over the picture instead of a black bar.

### Internal

- One shared `cam_show_cover()` helper drives both the viewer and the thumbnail
  (the thumbnail's contain-fit zoom math was generalised to cover). On-panel
  colour order / byte-swap stays hardware-only as before — only the framing
  changed, which the host sim renders faithfully.

## v0.15.1 — 2026-06

### Fixed

- **The camera never showed a frame** (neither the full-screen viewer nor the new
  Live thumbnail). Bambuddy's `/camera/snapshot` rejects the API key alone with
  401 — it requires a **stream token** from `POST /printers/camera/stream-token`,
  passed as `?token=`. The firmware never obtained one, so the camera could never
  work on the device. It now fetches + caches the token (≈60 min server TTL,
  refreshed early and re-fetched on a 401) before each snapshot.

### Internal

- The host simulator now **decodes the camera** (`stb_image`, pinned) and fetches
  one real frame after boot, so CI renders the actual picture and the Live
  thumbnail (`camera.png` + `dashboard.png`) instead of empty chrome — the last
  screen the sim couldn't validate. Failed fetches log the libcurl reason.

## v0.15.0 — 2026-06

### Added / Changed

- **Inline camera thumbnail on Live.** The printer-camera snapshot now also
  shows as a small live thumbnail in the top row (between the state pill and the
  ETA), tappable to open the existing full-screen viewer. It's revealed only once
  a frame has actually decoded — a Bambuddy without a camera leaves the dashboard
  exactly as before. The net task refreshes it while the Live screen is up (or
  the full-screen viewer is open) and backs off after repeated fetch failures.
- **Camera refresh slowed to every 5 s** (was 2 s) — gentler on the printer
  camera and the network for an always-on desk monitor.

### Notes

- The thumbnail picture is **hardware-only**: the host simulator stubs out JPEG
  decode, so CI can't render a frame — the layout degrades to the pre-existing
  dashboard there, and only a real device shows the live image.

## v0.14.1 — 2026-06

Hardening pass from a full-project audit — correctness/robustness/safety only
(the audit confirmed the v0.13/v0.14 AMS work itself is correct).

### Fixed

- **OTA refuses to flash an unverified image.** `check_and_update` aborts if the
  release manifest carries no valid 32-char `md5` — the manifest + binary travel
  over a `setInsecure()` TLS channel, so the MD5 bound into `Update.end()` is the
  integrity check. Previously a manifest without `md5` flashed unverified.
- **No boot reboot-loop when Wi-Fi is down.** With stored credentials but an
  unreachable AP (router/ISP outage), the device used to re-open the captive
  portal and `ESP.restart()` on its timeout, forever. It now boots into the
  normal UI offline with auto-reconnect armed; hold BOOT to re-provision.
- **Camera viewer no longer stalls ~6 s per frame** — the snapshot reader waited
  the full read timeout on chunked responses; it now ends a frame after a short
  idle gap once bytes arrive, freeing the net task.
- **AMS humidity tolerates float/string encodings** (was dropped to "unknown"
  unless a strict integer); RFID drying temp/time are clamped before the `uint8`
  cast so a garbage value can't wrap into a bogus setpoint.
- **Stale HMS line** on Live is cleared when all printers drop out of the list.

### Internal

- Net-task stack raised to 16 KB for the TLS + JSON + JPEG-decode call path; AMS
  unit-cycle index clamped to the chained-unit count; removed dead
  `clear_children()` and a duplicate i18n entry; corrected the misleading
  `LV_MEM_SIZE` comment (the pool is internal DRAM, not PSRAM).

## v0.14.0 — 2026-06

### Added

- **Filament-aware drying.** The Dry button no longer starts a fixed 60 min @
  55 °C cycle — it now derives the temperature and duration from the loaded
  filament. Bambu's per-spool RFID profile (`drying_temp` / `drying_time`) is
  used when present, with a per-type fallback table (PLA, PETG/PET, TPU, ABS,
  ASA, PA/nylon, PC, PVA, HIPS, PP/PPS) for spools without a tag. Because one
  heater warms the whole unit, the setpoint protects the most heat-sensitive
  loaded spool — the **lowest** recommended temperature across present slots,
  for the **longest** recommended time. The confirmation toast reports the
  actual values chosen (e.g. "Drying 8h @ 65 °C").

## v0.13.0 — 2026-06

AMS readability + drying reach. Both issues were seen on the host simulator
rendering real Bambuddy data (a 3-unit AMS 2 Pro with black PLA and clear PETG).

### Added

- **Dry / Stop on AMS 2 Pro.** The drying control was gated on AMS-HT only, so
  it never appeared on AMS 2 Pro units (they report `is_ams_ht=false` despite
  having a heater). It now shows on any heater-equipped unit — detected via
  HT-or-has-an-ambient-temperature — and stays hidden on the heater-less
  first-gen AMS.

### Fixed

- **Black and clear filament were indistinguishable (both drawn as panel grey).**
  Bambu reports tray colour as `RRGGBBAA`; we discarded the alpha, so clear PETG
  (`00000000`) and black PLA (`000000FF`) both became RGB `000000` — and the
  renderer additionally treated `000000` as "unknown" and painted it the panel
  highlight colour. Now: the alpha is parsed into a `translucent` flag, every
  swatch carries a 1 px outline so a black spool reads against the dark panel,
  real black renders as black, and translucent / "clear" filament renders as a
  light checkerboard (washed toward the filament's own tint when it has one).

## v0.12.1 — 2026-06

Bug-fix release — all three caught by the new host simulator running against a
real Bambuddy instance.

### Fixed

- **Live screen was blank on boot.** `ui::Manager::begin()` hid every screen
  then called `go_to(Dashboard)`, which early-returns because the manager
  already starts on Dashboard — so the Live screen never un-hid until you
  switched tabs. It now reveals the initial screen explicitly.
- **False full-screen HMS alert on healthy printers.** Bambu keeps low-severity
  HMS notifications in `hms_errors` even when nothing is wrong (Bambuddy's own
  UI ignores them); the device popped a red "HMS ERROR" overlay on a finished,
  fault-free print. HMS is now treated as active only when the printer is in a
  Failed / Error / Paused state.
- **Missing-glyph boxes.** The `•` (bullet) and `—` (em-dash) used as
  separators/placeholders aren't in the `bb_font` (Latin-1 + FontAwesome only)
  and rendered as tofu boxes; replaced with `·` (Latin-1 middot) and `-`.

### Internal

- **Host simulator** (`sim/`): a CI job builds the real LVGL UI + Bambuddy REST
  client against host shims and renders every screen to PNG artifacts on **real
  Bambuddy data** (via a Cloudflare Access service token), for visual validation
  without hardware. Not shipped to the device.

## v0.12.0 — 2026-06

Live printer camera on the device.

### Added

- **Camera snapshot viewer.** Tap the progress ring on Live to open a
  full-screen view of the printer's camera (Bambuddy `GET /camera/snapshot`).
  Frames are fetched + JPEG-decoded on the network task (with a native
  power-of-two downscale to fit the 480×272 panel) into a PSRAM buffer and
  refreshed roughly twice a second while the viewer is open; tap anywhere to
  close. Decode uses `bodmer/TJpg_Decoder`; the net task stack grew to 12 KB
  for the decoder workspace.

### Notes

- The viewer only does anything if your Bambuddy build exposes the camera
  endpoint and the printer has a reachable camera.
- Colour/byte order and decode scale are conservative defaults pending tuning
  on real hardware.

## v0.11.0 — 2026-06

Observability + robustness pass. No visual change to speak of; the device just
tells you more about itself and recovers better.

### Added

- **Wall-clock ETA.** Live now shows the estimated finish time of day
  ("ETA 1h12 · 14:32") alongside the countdown, from SNTP local time.
- **On-screen heap.** The Settings uptime row appends free heap — the cheapest
  at-a-glance signal that something's leaking.
- **Serial diagnostics.** Boot logs the reset cause (panic / brownout / watchdog
  vs a clean restart); the net task logs heap / PSRAM / WebSocket / RSSI /
  latency at the health cadence.
- **UI task watchdog.** A frozen screen now auto-recovers: the UI task feeds a
  10 s task watchdog and the device reboots itself if LVGL ever wedges. The
  HTTP-bound net task is intentionally left unwatched.

### Changed

- **JSON parsing moved to PSRAM.** All ArduinoJson documents (REST + WebSocket)
  allocate from PSRAM via a custom allocator instead of the scarce internal
  DRAM, so large `/status` payloads no longer pressure or fragment it.
- **Fewer flash writes.** Brightness changes only touch NVS when the level
  actually changed (was writing on every tap and every Settings re-sync).

## v0.10.0 — 2026-06

UI visual refresh. Same screens and controls, a more coherent and polished
look across all of them. Designed mockups-first (`docs/screenshots/`), then the
firmware was brought in line.

### Changed

- **Brand wordmark header.** Every screen now shows the **Bamboard** wordmark
  (with an accent mark + the focused printer + a Wi-Fi link badge) instead of
  the screen name — the bottom tab bar already marks the active screen, so the
  title was a duplicate.
- **Print speed is one button + a menu.** The always-on 4-segment speed chip on
  Live is replaced by a single button showing the current mode; tapping it opens
  a modal 4-item picker (Silent / Standard / Sport / Ludicrous). Frees up the
  action row and reads cleaner.
- **Depth + cohesion.** Cards get a subtle vertical gradient and a crisp
  hairline (no blur shadow — it would tax the ESP32 frame rate); the active
  chip, primary buttons and the progress ring use an accent gradient; the
  printer state shows as a dot+label pill; the active tab cell gets a dark-teal
  wash behind its accent icon/label.
- **Consistent geometry.** Unified 12 px content gutter everywhere (AMS was
  16 px); the three temperature cells are now equal width (were 142/142/156).
- **Refreshed mockups + hero render.** `docs/screenshots/*` and the README
  reflect the new look, with vector icons faithful to the FontAwesome glyphs
  the firmware renders.

### Notes

- No functional/behaviour change to printing, AMS control, OTA or networking —
  this release is cosmetic. All controls work exactly as before.

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
