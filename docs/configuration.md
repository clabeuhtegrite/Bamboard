# Configuration reference

Bamboard reads runtime configuration from NVS (ESP32's non-volatile
storage). All compile-time tunables live in `firmware/src/config.h`.

## Runtime settings (NVS)

| Key | Description                                                |
|-----|------------------------------------------------------------|
| `url` | Full Bambuddy base URL, including scheme and port (e.g. `http://192.168.1.42:8000`) |
| `key` | Bambuddy API key, sent as `X-API-Key` header             |

These are populated by the captive portal on first boot. To re-run the
portal, hold the side **BOOT** button at power-on.

## Compile-time tunables (`config.h`)

### Pin map

If you change the wiring (e.g. on a different ESP32 board), edit the
`pins` namespace. The display + GT911 touch pin map lives at the top of
`firmware/src/hw/display.cpp` (the LovyanGFX board class).

### Polling cadence

The defaults stay well under Bambuddy's 100 reads/minute limit:

| Constant | Default | Effect |
|----------|---------|--------|
| `POLL_DASHBOARD_MS` | 2000 ms | Frequency of `GET /printers/{id}/status` for the visible printer |
| `POLL_LIST_MS` | 5000 ms | Frequency of `GET /printers` (and per-printer status updates for the background ones) |
| `POLL_STATS_MS` | 30 s | Stats + recent archives |
| `POLL_HEALTH_MS` | 15 s | `GET /health` for the connectivity badge |

### UI palette

Bamboard's colour palette is in the `ui` namespace at the bottom of
`config.h`. Adjust freely — the entire UI is built from these constants.

### Auto-dim

The screen dims (not off) after `display::DIM_AFTER_MS` of inactivity.
Any touch wakes it.

### Scheduled reboot & clock

The `schedule` namespace controls the daily auto-reboot that lets the
boot-time OTA run unattended:

| Constant | Default | Effect |
|----------|---------|--------|
| `DAILY_REBOOT_ENABLED` | `true` | Master switch for the daily reboot |
| `DAILY_REBOOT_HOUR` | `0` | Local hour to reboot at (`0` = midnight) |
| `TZ` | Europe/Paris | POSIX TZ string for local time — **change this for your locale** |
| `NTP_SERVER1` / `NTP_SERVER2` | `pool.ntp.org` / `time.nist.gov` | SNTP sources |

The clock is synced over SNTP once Wi-Fi is up; the reboot only fires once the
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
