# Bamboard

> A budget, open-source touch desk monitor for your Bambu Lab printers
> — powered by [Bambuddy](https://github.com/maziggy/bambuddy).

Bamboard is a small ESP32-S3 device with a 4.3" capacitive touch screen
that lives on your desk and shows you, in real time, what your Bambu Lab
printers are doing. It talks **only** to your self-hosted Bambuddy
instance over its REST + WebSocket API — your printers never see a
third-party cloud.

Total BOM is around **20 €** (single all-in-one board, USB-C cable, 4
screws, ~70 g of filament for the case). The enclosure is FDM-printable
on any Bambu (or other) printer with a 0.4 mm nozzle.

![Bamboard assembled](docs/screenshots/device_render.svg)

| Live | AMS | Printers | History | Settings |
|------|-----|----------|---------|----------|
| ![Live](docs/screenshots/dashboard_mock.svg) | ![AMS](docs/screenshots/ams_mock.svg) | ![Printers](docs/screenshots/printers_mock.svg) | ![History](docs/screenshots/history_mock.svg) | ![Settings](docs/screenshots/settings_mock.svg) |

> The mockups above are from the v0.x button era and still illustrate
> the data each screen shows. The v1.0 touch UI keeps the same
> information density and grows a permanent bottom tab bar plus inline
> action buttons on Live; refreshed renders are on the v1.0 roadmap.

## Features

### What you see on the screen

- **Live dashboard** — nozzle / bed / chamber temperature, layer progress, ETA, filename.
- **AMS overview** — per-slot filament colour / type / remaining %, plus AMS humidity, temperature and active drying countdown. On-screen ◀ / ▶ buttons cycle through chained AMS / AMS-HT units.
- **Printers list** — every printer Bambuddy knows about, highlighted by state. **Tap a row** to focus that printer and jump to Live.
- **History & stats** — last 10 prints, success rate, total filament, total time.
- **Settings** — Bambuddy URL, local IP, Wi-Fi RSSI, uptime, plus a two-tap **Factory reset** button.

### Alerts

- **HMS surfacing** — the HMS string is shown in red on the dashboard, and a full-screen pulsing red overlay pops every 30 s while the error persists. Tap anywhere to dismiss; the overlay re-arms after the cooldown so the alert can't be silenced indefinitely.

### Controls (touch-first)

- **Bottom tab bar** — 5 tabs always visible (Live / AMS / Printers / History / Settings). Tap to switch.
- **Swipe gesture** — swipe left / right inside the screen body to move to the next / previous tab.
- **Inline action buttons on Live** — appear contextually based on the focused printer's state:
  - while printing → `Speed: <current> → <next>` (tap to cycle Silent / Standard / Sport / Ludicrous);
  - when finished → `Clear plate`;
  - while an HMS error is active → `Clear HMS`.
- **Factory reset** — Settings → "Factory reset" → tap a second time within 3 s to confirm. Wipes Wi-Fi + Bambuddy creds and reboots into the captive portal.

### Connectivity

- **WebSocket push** — subscribes to Bambuddy's `/ws` for real-time `printer_status` frames. REST polling stays as a 30 s safety net (vs. 2 s when WS is down). Cuts HMS-alert latency from a poll cycle to one network hop.
- **Wi-Fi captive portal** — first-boot Wi-Fi + Bambuddy URL + API key setup; no re-flash needed to change them. Hold the side **BOOT** button at boot to re-run it.
- **Auto-dim** — backlight drops after 60 s without a touch, wakes on the next tap.

### Install & updates

- **One-click USB flash** — `scripts/flash-windows.bat`, `scripts/flash-mac.command`, `scripts/flash-linux.sh`. Auto-detects the serial port via VID:PID scoring.
- **One-click OTA updates** — `scripts/update-*` discovers the device via mDNS (with cache + IP fallback) and pushes the new firmware. Full-screen progress overlay on the device during upload.
- **OTA password protection** — default `bamboard`, overridable via `BAMBOARD_OTA_PASSWORD` or a build flag. See [docs/flashing.md](docs/flashing.md).

### Development

- **Desktop sim** — `sim/` ships a CMake + SDL2 harness that runs the same `firmware/src/ui/{screens,ui}.cpp` sources in a native 480 × 272 window so the UI can be tweaked without flashing. Mouse = touch. See [sim/README.md](sim/README.md).

## Repo layout

```
.
├── firmware/        PlatformIO project (ESP32-S3 + LVGL + LovyanGFX)
│   ├── src/         C++ sources
│   │   ├── hw/      Display + GT911 touch HAL (LovyanGFX-based)
│   │   ├── net/     Bambuddy REST + WebSocket clients
│   │   ├── ui/      LVGL screen manager + per-screen builders
│   │   ├── config.h All compile-time tunables
│   │   └── main.cpp Boot, Wi-Fi provisioning, FreeRTOS tasks, ArduinoOTA
│   ├── include/     lv_conf overrides
│   └── platformio.ini
├── sim/             Desktop LVGL preview (CMake + SDL2)
├── scripts/         One-click flash / update launchers (Windows/macOS/Linux)
├── hardware/        Bill of materials, wiring diagram
├── case/            Parametric OpenSCAD enclosure + STL exports
└── docs/            Assembly, flashing, configuration guides
```

## Quick start

1. Order the parts from `hardware/bom.md` (~20 €).
2. Print the enclosure: `case/bamboard.scad` → export STL → slice → print PLA / PETG.
3. Drop the Guition board into the front shell, clip the back shell on,
   drive four M3 × 8 mm screws. (No wiring — everything's on the one PCB.)
4. Flash the firmware: plug the board in via USB-C, then double-click
   `scripts/flash-windows.bat`, `scripts/flash-mac.command` or run
   `scripts/flash-linux.sh`. See `docs/flashing.md` for details.
5. First boot: device exposes a `Bamboard-setup` Wi-Fi AP, connect, fill
   in your Wi-Fi credentials and your Bambuddy URL + API key via the
   captive portal.
6. Enjoy.

## Requirements

- A running Bambuddy instance (see https://github.com/maziggy/bambuddy)
- A Bambuddy API key with at least `printers:read` and `statistics:read`
  permissions (add `printers:control` if you want to use the inline
  Speed / Clear plate / Clear HMS actions).

## License

MIT — see [LICENSE](LICENSE). Bamboard is not affiliated with Bambu Lab
or with the Bambuddy project.
