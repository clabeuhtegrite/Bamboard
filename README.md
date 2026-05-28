# Bamboard

> A budget, open-source desk monitor for your Bambu Lab printers — powered by
> [Bambuddy](https://github.com/maziggy/bambuddy).

Bamboard is a small ESP32-S3 device with a 4" colour screen and three buttons
that lives on your desk and shows you, in real time, what your Bambu Lab
printers are doing. It talks **only** to your self-hosted Bambuddy instance
over its REST API — your printers never see a third-party cloud.

Total BOM is around **30 €** (display included). The enclosure is FDM-printable
on any Bambu (or other) printer with 0.4 mm nozzle.

![Bamboard assembled](docs/screenshots/device_render.svg)

| Live | AMS | Printers | History | Settings |
|------|-----|----------|---------|----------|
| ![Live](docs/screenshots/dashboard_mock.svg) | ![AMS](docs/screenshots/ams_mock.svg) | ![Printers](docs/screenshots/printers_mock.svg) | ![History](docs/screenshots/history_mock.svg) | ![Settings](docs/screenshots/settings_mock.svg) |

## Features

### What you see on the screen

- **Live dashboard** — nozzle / bed / chamber temperature, layer progress, ETA, filename.
- **AMS overview** — per-slot filament colour / type / remaining %, plus AMS humidity, temperature and active drying countdown. Cycles through chained AMS / AMS-HT units.
- **History & stats** — last 10 prints, success rate, total filament, total time.
- **Printers list** — every printer Bambuddy knows about, highlighted by state.
- **Settings** — Bambuddy URL, local IP, Wi-Fi RSSI, uptime, build info.

### Alerts

- **HMS surfacing** — the HMS string is shown in red on the dashboard, the status LED breathes red, and a full-screen flashing overlay pops every 30 s while the error persists (dismissable with any button — re-arms on the next cooldown so the alert can't be silenced indefinitely).
- **Status LED** — single WS2812 RGB module, screwless plug-in, state-driven ambient patterns.

### Controls

- **Live quick actions** — double-click OK on the Live screen for a contextual popup: "Cycle speed" while printing, "Clear plate" when finished, plus "Clear HMS" always. Saves a trip to the Printers screen.
- **Per-printer actions menu** — long-press OK on the Printers screen for the same modal, targeting any printer in the list.
- **Print speed** — Silent / Standard / Sport / Ludicrous, via long-press OK on Live or the quick-actions popup.
- **Multi-printer cycling** — long-press PREV/NEXT on Dashboard / AMS / Printers switches the focused printer.

### Connectivity

- **WebSocket push** — subscribes to Bambuddy's `/ws` for real-time `printer_status` frames. REST polling stays as a 30 s safety net (vs. 2 s when WS is down). Cuts HMS-alert latency from a poll cycle to one network hop.
- **Wi-Fi captive portal** — first-boot Wi-Fi + Bambuddy URL + API key setup, no re-flash needed to change them.
- **Auto-dim** — screen dims after inactivity to save the backlight.

### Install & updates

- **One-click USB flash** — double-click the launcher for your OS (`scripts/flash-windows.bat`, `flash-mac.command`, `flash-linux.sh`). Auto-detects the serial port via VID:PID scoring.
- **One-click OTA updates** — equivalent launcher under `scripts/update-*` discovers the device via mDNS (with cache + IP fallback) and pushes the new firmware. Full-screen progress overlay on the device during upload.
- **OTA password protection** — default `bamboard`, overridable via `BAMBOARD_OTA_PASSWORD` or a build flag. See [docs/flashing.md](docs/flashing.md).

### Roadmap

_Nothing pinned at the moment — everything originally claimed by the project is now shipped. Open an issue if you want a feature considered._

## Repo layout

```
.
├── firmware/        PlatformIO project (ESP32-S3 + LVGL + TFT_eSPI)
│   ├── src/         C++ sources
│   │   ├── hw/      Buttons, display, status LED drivers
│   │   ├── net/     Bambuddy REST + WebSocket clients
│   │   ├── ui/      LVGL screen manager + per-screen builders
│   │   ├── config.h All compile-time tunables
│   │   └── main.cpp Boot, Wi-Fi provisioning, FreeRTOS tasks, ArduinoOTA
│   ├── include/     TFT_eSPI User_Setup + lv_conf overrides
│   └── platformio.ini
├── scripts/         One-click flash / update launchers (Windows/macOS/Linux)
├── hardware/        Bill of materials, wiring diagram
├── case/            Parametric OpenSCAD enclosure + STL exports
└── docs/            Assembly, flashing, configuration guides
```

## Quick start

1. Order the parts from `hardware/bom.md` (~30 €).
2. Print the enclosure: `case/bamboard.scad` → export STL → slice → print PLA / PETG.
3. Wire it up as shown in `hardware/wiring.md` (no soldering required).
4. Flash the firmware: plug the ESP32-S3 in via USB-C, then double-click
   `scripts/flash-windows.bat`, `scripts/flash-mac.command` or run
   `scripts/flash-linux.sh`. See `docs/flashing.md` for details.
5. First boot: device exposes a `Bamboard-setup` Wi-Fi AP, connect, fill in
   your Wi-Fi credentials and your Bambuddy URL + API key.
6. Enjoy.

## Requirements

- A running Bambuddy instance (see https://github.com/maziggy/bambuddy)
- A Bambuddy API key with at least `printers:read` and `statistics:read` permissions
  (add `printers:control` if you want to use the speed / clear-plate actions)

## License

MIT — see [LICENSE](LICENSE). Bamboard is not affiliated with Bambu Lab or with
the Bambuddy project.
