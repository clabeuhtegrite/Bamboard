# Bamboard

> A budget, open-source desk monitor for your Bambu Lab printers — powered by
> [Bambuddy](https://github.com/maziggy/bambuddy).

Bamboard is a small ESP32-S3 device with a 4" colour screen and three buttons
that lives on your desk and shows you, in real time, what your Bambu Lab
printers are doing. It talks **only** to your self-hosted Bambuddy instance
over its REST API — your printers never see a third-party cloud.

Total BOM is around **30 €** (display included). The enclosure is FDM-printable
on any Bambu (or other) printer with 0.4 mm nozzle.

| Live | Printers | History | Settings |
|------|----------|---------|----------|
| ![Live](docs/screenshots/dashboard_mock.svg) | ![Printers](docs/screenshots/printers_mock.svg) | ![History](docs/screenshots/history_mock.svg) | ![Settings](docs/screenshots/settings_mock.svg) |

## Features

- **Live dashboard** — nozzle / bed / chamber temperature, layer progress, ETA, filename
- **Multi-printer** — switch instantly between all printers known to Bambuddy
- **History & stats** — last 10 prints, success rate, total filament, total time
- **HMS alerts** — when Bambuddy reports an HMS error the screen flashes and the LED turns red
- **AMS overview** — currently loaded filaments per slot with colour swatches
- **Print speed control** — change the speed preset (Silent / Standard / Sport / Ludicrous) from the device
- **Quick actions** — refresh status, acknowledge plate cleared, clear HMS errors
- **Wi-Fi captive portal** — first-boot setup, no need to reflash to change credentials
- **Status LED** — single WS2812 RGB module, screwless plug-in
- **Auto-dim** — screen dims after inactivity to save the backlight
- **OTA-ready** — push new firmware over your LAN once configured

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
