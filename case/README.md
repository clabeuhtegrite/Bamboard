# Bamboard enclosure

The enclosure is **not Bamboard's own design** — it's the excellent
**"ESP32-S3 Klipper Dashboard Case"** by **TomE1337**, which fits the same
Guition JC4827W543 board Bamboard runs on. Big thanks to TomE1337 for
designing and sharing it.

- **Designer:** TomE1337 — <https://www.printables.com/@TomE1337_2178164>
- **Model:** *ESP32-S3 Klipper Dashboard Case with Weather, Crypto & Moonraker*
  — <https://www.printables.com/model/1716582-esp32-s3-klipper-dashboard-case-with-weather-crypt>
- **License:** Creative Commons **Attribution-NonCommercial-ShareAlike 4.0**
  (CC BY-NC-SA 4.0) — <https://creativecommons.org/licenses/by-nc-sa/4.0/>

The STLs here are **redistributed unmodified** under that license — see
[`LICENSE`](LICENSE). **The Printables page is the source of truth** for the
recommended print profile, orientation and any hardware (screws / heat-set
inserts); follow it for the authoritative build.

## Parts

| File        | Role (as shipped)            | Bounding box (mm)   |
|-------------|------------------------------|---------------------|
| `case.stl`  | Main body — holds the board + screen | 129 × 79 × 41 |
| `cover.stl` | Rear cover                   | 125 × 74 × 15       |
| `stand.stl` | Desk stand the body sits in  | 139 × 63 × 52       |

*(Roles are inferred from the model and the product photo; defer to the
Printables page for the designer's own description.)*

## Printing — generic guidance

These are sensible defaults for a desk enclosure; **TomE1337's Printables
page lists the recommended settings**, so prefer those if they differ.

| Setting       | Value                                                            |
|---------------|-----------------------------------------------------------------|
| Nozzle        | 0.4 mm                                                          |
| Layer height  | 0.20 mm                                                         |
| Walls         | 3 perimeters                                                    |
| Infill        | 15–20 %                                                         |
| Filament      | **PETG preferred** — the panel runs slightly warm; PLA is fine away from direct sun |
| Supports      | Per the Printables page (orientation-dependent)                 |

## Assembly

The board is the same all-in-one Guition JC4827W543 as before (no wiring,
no soldering — see [`../hardware/bom.md`](../hardware/bom.md)). Seat the
board in `case.stl`, close it with `cover.stl`, and drop the assembly into
`stand.stl`. **For the exact fit, fastener list and step-by-step, follow
the [Printables page](https://www.printables.com/model/1716582-esp32-s3-klipper-dashboard-case-with-weather-crypt).**

## License & attribution

Everything in this `case/` directory is © **TomE1337** and licensed
**CC BY-NC-SA 4.0**, which is **separate** from the MIT license covering
Bamboard's own code. If you remix the enclosure you must credit TomE1337
and keep the same license; **commercial use is not permitted**. See
[`LICENSE`](LICENSE) for the full attribution.
