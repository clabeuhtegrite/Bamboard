# Configuration reference

Bamboard reads runtime configuration from NVS (ESP32's non-volatile
storage). All compile-time tunables live in `firmware/src/config.h`.

## Runtime settings (NVS)

| Key | Description                                                |
|-----|------------------------------------------------------------|
| `url` | Full Bambuddy base URL, including scheme and port (e.g. `http://192.168.1.42:8000`) |
| `key` | Bambuddy API key, sent as `X-API-Key` header             |
| `tz` | POSIX timezone string for the clock / daily reboot (e.g. `CET-1CEST,M3.5.0,M10.5.0/3`) |
| `reboot_h` | Daily reboot hour in local time (`0`–`23`); `255` = disabled |
| `lang` | UI language index — `0` en, `1` es, `2` fr, `3` pt, `4` de |
| `bl_level` | Screen brightness 1–5 (set on the Settings screen, not the portal) |

`url`, `key`, `tz`, `reboot_h` and `lang` are populated by the captive portal on
first boot. To change them later, tap **Wi-Fi setup** on the Settings screen —
it re-opens the portal with everything pre-filled and saves a one-shot `reprov`
flag in NVS, so it reboots straight into the portal **without wiping**. That
lets you move the device to a new Wi-Fi network without re-entering your
Bambuddy host / API key / Cloudflare token. Holding the side **BOOT** button at
power-on also re-runs the portal, but **wipes** all settings first (same as the
Settings → Factory reset button).

### Interface language

The captive portal's **Language** field takes a two-letter code — `en`
(English), `es` (Español), `fr` (Français), `pt` (Português) or `de` (Deutsch).
It's applied at boot before the screens are built. Adding a language means
extending the `i18n` table (`firmware/src/ui/i18n.*`); only Latin-script
languages work with the bundled `bb_font_*` fonts.

## Compile-time tunables (`config.h`)

### Pin map

If you change the wiring (e.g. on a different ESP32 board), edit the
`pins` namespace. The display + GT911 touch pin map lives at the top of
`firmware/src/hw/display.cpp` (the LovyanGFX board class).

### Polling cadence

The defaults stay well under Bambuddy's 100 reads/minute limit:

| Constant | Default | Effect |
|----------|---------|--------|
| `POLL_DASHBOARD_MS` | 2000 ms | Per-printer `GET /printers/{id}/status` cadence when the WebSocket is **down** — polls **every** printer, not just the focused one |
| `POLL_DASHBOARD_WS_MS` | 30 s | Focused-printer status poll when the WebSocket **is** connected (slow safety net behind the live push) |
| `POLL_LIST_MS` | 5000 ms | `GET /printers` — the identity list |
| `POLL_STATS_MS` | 30 s | Stats + recent archives |
| `POLL_QUEUE_MS` | 15 s | Print-queue refresh |
| `POLL_HEALTH_MS` | 15 s | `GET /health` for the connectivity badge |
| `POLL_SYSINFO_MS` | 60 s | Bambuddy server version + uptime |

### UI palette

Bamboard's colour palette is in the `ui` namespace at the bottom of
`config.h`. Adjust freely — the entire UI is built from these constants.

### Auto-dim

The screen dims (not off) after `display::DIM_AFTER_MS` of inactivity.
Any touch wakes it.

### Scheduled reboot & clock

The daily auto-reboot lets the boot-time OTA run unattended. The **timezone**
and **reboot hour** are set in the captive portal (stored in NVS as `tz` /
`reboot_h`, see above) — the `schedule` namespace in `config.h` only holds the
first-boot defaults and the fixed pieces:

| Constant | Default | Effect |
|----------|---------|--------|
| `DAILY_REBOOT_ENABLED` | `true` | First-boot default for whether the reboot is on |
| `DAILY_REBOOT_HOUR` | `0` | First-boot default hour — the portal's `reboot_h` overrides it |
| `TZ` | Europe/Paris | First-boot default timezone — the portal's `tz` overrides it |
| `NTP_SERVER1` / `NTP_SERVER2` | `pool.ntp.org` / `time.nist.gov` | SNTP sources |

Leave the portal's reboot-hour field blank to disable the daily reboot. The
clock is synced over SNTP once Wi-Fi is up; the reboot only fires once the
clock is valid. On that reboot, the boot-time OTA check (the `ota` namespace)
pulls and flashes any newer release — so the device stays current overnight
without a prompt or a power-cycle.

## Bambuddy-side setup

1. Install Bambuddy from https://github.com/maziggy/bambuddy and add at
   least one printer in the web UI.
2. Create an API key: Settings → API Keys → Create.
3. Grant the key these permissions:
   - `printers:read` *(required)*
   - `statistics:read` *(for the History screen)*
   - `archives:read` *(for the recent prints list)*
   - `printers:control` *(only if you want the speed-change and refresh
     actions to work; safe to omit if you want a strictly read-only device)*
4. Copy the key (it's shown only once) and paste it into the Bamboard
   captive portal.

## Webhook bonus (optional)

Bamboard polls — it does not subscribe to webhooks. If you want
near-instant push updates instead, configure a webhook in Bambuddy that
hits a small relay you write yourself. We deliberately kept Bamboard
poll-only to avoid running an HTTP server on the device, which would add
flash size and an attack surface for ~no UX gain at 2 s poll cadence.
