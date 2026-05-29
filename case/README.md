# Bamboard enclosure (v1.0)

Parametric OpenSCAD model. **Two parts** — both printable in PLA or PETG
on a 0.4 mm nozzle:

| Part           | What it holds / does                                    | Qty |
|----------------|----------------------------------------------------------|-----|
| `front_shell`  | Display viewing window + friction-fit pocket for the Guition JC4827W543 PCB. Optional M3 corner standoffs that align with the board's own mounting holes. | 1 |
| `back_shell`   | USB-C cut-out, BOOT + RST button windows, microSD slot, vent ribs, four corner screw bosses. | 1 |

The shells clip together with **four M3 × 8 mm self-tapping screws**.

> **Heads-up — print a draft first.**
> The JC4827W543 has shipped in several mechanical revisions. The default
> board outline (108 × 78 mm) and connector offsets match the most-common
> 2024–2025 batch; if your specific board is even a millimetre off, tune
> `board_w / board_h / board_hole_pitch_* / usb_y_offset / boot_btn_y`
> at the top of `bamboard.scad`. Everything in the model is derived from
> those — change a number, re-render, re-print just the front in 30 min.

## Render & export STL

```bash
# Front shell (display window + board pocket)
openscad -D 'part="front"' -o exports/bamboard_front.stl bamboard.scad

# Back shell (USB / BOOT / RST / SD / vents)
openscad -D 'part="back"'  -o exports/bamboard_back.stl  bamboard.scad

# Both on the same bed for a visual sanity check
openscad -D 'part="all"'   -o exports/bamboard_all.stl   bamboard.scad
```

(or open `bamboard.scad` in the OpenSCAD GUI, toggle `part` at the top,
F6 → File → Export → STL.)

## Print settings

| Setting              | Value           |
|----------------------|-----------------|
| Layer height         | 0.20 mm         |
| Nozzle               | 0.4 mm          |
| Perimeters           | 3               |
| Top / bottom layers  | 4               |
| Infill               | 20 % gyroid     |
| Supports             | Front: none. Back: only on the USB / BOOT cut-outs (auto-painted is enough) |
| Filament             | PETG preferred (the panel runs slightly warm). PLA is fine if your desk doesn't see direct sun. |

Total print time on a Bambu X1 Carbon at default profile: ~2 h 30 for
both shells, ~50 g of filament.

## Print orientation

- **Front shell** → face-down on the bed (the flat, screen-facing side
  becomes the visible outer surface). No supports needed.
- **Back shell** → face-down on the bed. Minor overhangs on the USB /
  BOOT / SD cut-outs; auto-painted supports cover them.

## Assembly (zero soldering, single screwdriver)

1. **Drop the Guition board into the front shell**: the friction-fit
   pocket guides it; the four M3 standoffs (if you chose to print them)
   align with the board's mounting holes. Press lightly until the FPC
   shielding kisses the inside of the front face.
2. **Optional**: secure the board with 4 × M2 × 4 mm screws into the
   standoffs. Foam tape is fine too.
3. **Clip the back shell on top**. The USB-C connector lines up with the
   right-edge cut-out; the BOOT and RST buttons poke through their own
   circular windows so they stay reachable for factory-reset and OTA
   bootloader entry.
4. Drive **four M3 × 8 mm self-tapping screws** through the back into
   the front-shell corner bosses.
5. Plug a USB-C cable into the side. That's the whole build.

## Adapting to a different revision

Variables at the top of `bamboard.scad` cover the common deviations:

| Variable                              | What to set                                              |
|---------------------------------------|----------------------------------------------------------|
| `board_w` / `board_h`                 | PCB outline (calipers).                                  |
| `board_hole_pitch_x / _y`             | Mounting hole pattern. Set `board_hole_d = 0` to skip.   |
| `view_w` / `view_h` / `view_off_y`    | Visible active area of the display + its position on PCB.|
| `usb_y_offset`                        | USB-C connector distance from the bottom short edge.     |
| `boot_btn_y` / `reset_btn_y`          | BOOT / RST button positions on the side rail.            |
| `sd_y_offset`                         | microSD slot position (skip the cut-out if absent).      |
| `wall` / `top_thickness`              | Sturdier (3.0 mm) or lighter (1.8 mm) shells.            |

If a connector isn't on your board (e.g. no microSD), comment its cut-out
out of `back_shell()` or set its `y_offset` so far outside the case
outline that the cut-out lands harmlessly in the wall — Boolean
subtraction with nothing to cut is free.
