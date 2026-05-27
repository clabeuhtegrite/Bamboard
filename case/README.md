# Bamboard enclosure

Parametric OpenSCAD model. Two shells (front + back) and an optional
button cap, all printable in PLA or PETG on a 0.4 mm nozzle with no
supports for the front and minor supports for the back.

## Render & export STL

```bash
# Front
openscad -D 'part="front"' -o exports/bamboard_front.stl bamboard.scad

# Back
openscad -D 'part="back"' -o exports/bamboard_back.stl bamboard.scad

# Button cap (print 3×)
openscad -D 'part="buttoncap"' -o exports/bamboard_buttoncap.stl bamboard.scad
```

(or just open `bamboard.scad` in the OpenSCAD GUI, toggle `part` at the
top of the file, and hit F6 → File → Export → STL.)

## Print settings

| Setting              | Value           |
|----------------------|-----------------|
| Layer height         | 0.20 mm         |
| Nozzle               | 0.4 mm          |
| Perimeters           | 3               |
| Top / bottom layers  | 4               |
| Infill               | 20 % gyroid     |
| Supports             | Front: none. Back: only on the rails (auto) |
| Filament             | PLA or PETG. PETG preferred for the front shell, since it sits next to the warm display. |

Total print time on a Bambu X1 Carbon at default profile: ~3 h, ~80 g.

## Adapting to a different panel

Variables in the first 60 lines of `bamboard.scad` cover the common
deviations:

- `disp_pcb_w` / `disp_pcb_h`: outline of *the PCB*, not of the visible
  area. Measure with a caliper, with the FFC ribbon side included.
- `disp_view_w` / `disp_view_h` and the `_off_x` / `_off_y` pair: visible
  area and its position on the PCB.
- `disp_hole_pitch_*` and `disp_hole_d`: mounting hole pattern of the PCB.

If your panel's mounting holes don't line up, simply set
`disp_hole_pitch_x = 0` and use double-sided foam tape instead — there's
a 0.4 mm relief in the front shell to seat the panel.
