# Bamboard enclosure (v0.4)

Compact two-part FDM-printable case for the Guition JC4827W543 board.
Both parts print on a 0.4 mm nozzle in PLA or PETG (PETG preferred — the
panel runs warm enough that you'll see PLA soften after a couple of
weeks on a sunny desk).

| Part           | What it holds / does                                                                                                                | Qty |
|----------------|--------------------------------------------------------------------------------------------------------------------------------------|-----|
| `front_shell`  | Slim 3 mm bezel around the active area, snug PCB pocket (0.3 mm clearance per side), four corner screw bosses, optional M3 standoffs aligning with the board's mounting holes. Chamfered top edge so the case reads as one piece. | 1 |
| `back_shell`   | USB-C cut-out, BOOT + RST button windows, microSD slot, vertical vent slots grouped over the ESP module, four corner screw posts. Integrated 15° desk-stand tab — no accessory needed.                                            | 1 |

The shells screw together with **four M3 × 6 mm self-tapping screws**.
(v0.3 used M3 × 8 mm — the new shells are thinner so the shorter screw
seats fully without poking through.)

## What changed since v0.3

| Dimension              | v0.3    | v0.4    |
|------------------------|---------|---------|
| Outer width            | 122 mm  | 112 mm  |
| Outer height           | 92 mm   | 82 mm   |
| Wall thickness         | 2.4 mm  | 1.8 mm  |
| Top thickness          | 2.0 mm  | 1.6 mm  |
| Bezel around screen    | ~6 mm   | ~3 mm   |
| PCB pocket clearance   | 6 mm    | 0.6 mm  |
| Front-edge geometry    | flat    | 0.6 mm chamfer |
| Desk stand             | accessory | integrated tab @ 15° |
| Total filament         | ~50 g   | ~32 g   |

> **Heads-up — print a draft first.**
> The JC4827W543 has shipped in several mechanical revisions. The
> default board outline (108 × 78 mm) and connector offsets match the
> most-common 2024–2025 batch; if your specific board is even a
> millimetre off, tune `board_w / board_h / board_hole_pitch_* /
> usb_y_offset / boot_btn_y` at the top of `bamboard.scad`. The new
> snug pocket gives you almost no wiggle room — measure before slicing.

## Render & export STL

```bash
# Front shell (display window + board pocket)
openscad -D 'part="front"' -o exports/bamboard_front.stl bamboard.scad

# Back shell (USB / BOOT / RST / SD / vents + stand)
openscad -D 'part="back"'  -o exports/bamboard_back.stl  bamboard.scad

# Both on the same bed for a visual sanity check
openscad -D 'part="all"'   -o exports/bamboard_all.stl   bamboard.scad
```

(or open `bamboard.scad` in the OpenSCAD GUI, toggle `part` at the top,
F6 → File → Export → STL.)

## Print settings

| Setting              | Value                                                                                              |
|----------------------|----------------------------------------------------------------------------------------------------|
| Layer height         | 0.20 mm                                                                                            |
| Nozzle               | 0.4 mm                                                                                             |
| Perimeters           | 3                                                                                                  |
| Top / bottom layers  | 4                                                                                                  |
| Infill               | 20 % gyroid                                                                                        |
| Supports             | None — both parts print face-down, and the back shell's stand tab is a single 15° overhang that bridges without supports. |
| Filament             | PETG preferred (panel runs slightly warm). PLA is fine if your desk doesn't see direct sun.        |
| Ironing              | **On** for the front shell's first layer — that's the visible bezel face.                          |

Total print time on a Bambu X1 Carbon at default profile: **~1 h 40
both shells, ~32 g of filament**.

## Print orientation

- **Front shell** → face-down on the bed (the flat, screen-facing side
  becomes the visible outer surface). No supports needed.
- **Back shell** → face-down on the bed. The integrated stand tab tucks
  underneath in this orientation and prints clean without supports.

## Assembly (zero soldering, single screwdriver)

1. **Drop the Guition board into the front shell**: the friction-fit
   pocket guides it; the four M3 standoffs (if you chose to print them)
   align with the board's mounting holes. Press lightly until the FPC
   shielding kisses the inside of the front face.
2. **Optional**: secure the board with 4 × M2 × 4 mm screws into the
   standoffs. Foam tape is fine too — the new tight pocket means the
   board doesn't move regardless.
3. **Clip the back shell on top**. The USB-C connector lines up with
   the right-edge cut-out; the BOOT and RST buttons poke through their
   own circular windows so they stay reachable for factory-reset and
   OTA bootloader entry.
4. Drive **four M3 × 6 mm self-tapping screws** through the back into
   the front-shell corner bosses.
5. Plug a USB-C cable into the side. That's the whole build.

The integrated stand tab pivots out at the bottom of the back shell.
Press it open to ~90° from the case body and the screen will tilt back
15° toward you — comfortable viewing from a seated desk position.

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
| `wall` / `top_thickness`              | Sturdier (2.4 mm) or lighter (1.5 mm) shells.            |
| `pocket_clear`                        | Pocket clearance on every side. Bump to 1.0 mm if your panel is mounted out-of-square. |
| `stand_enabled`                       | `false` to skip the desk-stand tab (wall-mount / printer-cart use cases). |
| `stand_tilt_deg`                      | Stand angle. 15° works for a seated desk; bump to 25° for a low table. |

If a connector isn't on your board (e.g. no microSD), comment its
cut-out out of `back_shell()` or set its `y_offset` so far outside the
case outline that the cut-out lands harmlessly in the wall — Boolean
subtraction with nothing to cut is free.
