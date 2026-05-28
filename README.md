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

### Shipped in v0.1

- **Live dashboard** — nozzle / bed / chamber temperature, layer progress, ETA, filename
- **AMS overview** — per-slot filament colour / type / remaining %, plus AMS humidity, temperature and active drying countdown; long-press OK to cycle through chained AMS / AMS-HT units
- **Multi-printer** — switch between all printers known to Bambuddy with the buttons
- **History & stats** — last 10 prints, success rate, total filament, total time
- **Print speed control** — cycle the speed preset (Silent / Standard / Sport / Ludicrous) by long-pressing OK on the live screen
- **HMS surfacing** — the HMS string is shown in red on the dashboard and the status LED breathes red
- **Per-printer actions menu** — long-press OK on the Printers screen opens a modal to clear HMS errors or acknowledge a cleared build plate (so the scheduler starts the next queued print)
- **Wi-Fi captive portal** — first-boot Wi-Fi + Bambuddy URL + API key setup, no re-flash needed to change them
- **Status LED** — single WS2812 RGB module, screwless plug-in, state-driven ambient patterns
- **Auto-dim** — screen dims after inactivity to save the backlight

### Roadmap (claimed earlier but not yet wired)

- **Full-screen HMS flash** — currently only the HMS label turns red; a periodic full-screen warning overlay is TODO
- **OTA upload** — `pio run -t upload --upload-port bamboard.local` documented but `ArduinoOTA.begin()` not yet called in `setup()`
- **WebSocket events** — Bambuddy exposes `/ws` for push events; switching from polling would make HMS alerts near-instant. Currently we poll every 2 s, which is fine for desk-side use

## Repo layout

```
.
├── firmware/        PlatformIO project (ESP32-S3 + LVGL + TFT_eSPI)
│   ├── src/         C++ sources (UI, network, hardware)
│   ├── include/     TFT_eSPI User_Setup overrides
│   └── platformio.ini
├── hardware/        Bill of materials, wiring diagram
├── case/            Parametric OpenSCAD enclosure + STL exports
└── docs/            Assembly, flashing, configuration guides
```

## Quick start

1. Order the parts from `hardware/bom.md` (~30 €).
2. Print the enclosure: `case/bamboard.scad` → export STL → slice → print PLA / PETG.
3. Wire it up as shown in `hardware/wiring.md` (no soldering required).
4. Flash the firmware: see `docs/flashing.md`.
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
