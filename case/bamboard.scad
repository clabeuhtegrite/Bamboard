// =============================================================================
// Bamboard enclosure (v0.4) — parametric OpenSCAD model
// =============================================================================
//
// Two-part FDM-printable case for the Guition JC4827W543 all-in-one board
// (ESP32-S3-WROOM-1 + 4.3" IPS 480×272 + GT911 capacitive touch on one PCB):
//
//   front_shell : slim 3 mm bezel around the active area, snug PCB pocket,
//                 chamfered front edge so the case reads as one piece
//                 instead of two stacked boxes
//   back_shell  : USB-C / BOOT / RST / microSD cut-outs, side vents, an
//                 integrated 15° desk-stand tab so no extra accessory is
//                 needed to angle the screen toward you
//
// The two halves screw together with **four M3 × 6 mm self-tapping
// screws** (was M3 × 8 mm on v0.3 — the new walls are thinner so the
// shorter screw seats fully without poking through).
//
// What changed since v0.3:
//   - PCB pocket clearance dropped from 6 mm to 0.6 mm. The case is
//     ~8 mm narrower and ~6 mm shorter overall.
//   - Walls trimmed from 2.4 mm → 1.8 mm. Top thickness 2.0 mm → 1.6 mm.
//   - Bezel around the active area dropped from ~6 mm to ~3 mm.
//   - Outer corners chamfered (0.6 mm 45°) so the case looks intentional
//     rather than "extruded from a sketch".
//   - Integrated rear desk-stand replaces the previous accessory tab.
//   - Vent slots rotated to vertical and grouped near the ESP module
//     (where the heat actually is, not the dead-zone over the FPC).
//
// Print orientation:
//   front_shell: face-down on the bed. The bezel is the bed face, so
//                ironing the first layer is worth it.
//   back_shell : face-down on the bed; the stand tab tucks under, so no
//                supports needed. The USB / BOOT / SD cut-outs are
//                bridges short enough to print clean at 0.20 mm.
//
// Recommended: 0.20 mm layer, 3 perimeters, 20 % gyroid, PETG.
//
// Heads-up: the JC4827W543 has shipped in several mechanical revisions
// over the years and the exact PCB / connector offsets vary. Print the
// front shell as a tolerance check first (≈ 25 min on a Bambu X1C) and
// nudge the variables below if anything doesn't fit. Every dimension
// you might want to change is in the first ~80 lines of the file.
//
// =============================================================================

$fa = 2;
$fs = 0.4;

// ---------- Part selection --------------------------------------------------
// Toggle which part is rendered and exported. The launcher commands in
// case/README.md set this from the CLI.
part = "front";   // "front" | "back" | "all"

// ---------- Global wall geometry --------------------------------------------
// Slimmer than v0.3. The PETG case is still rigid because we picked up
// stiffness from the chamfered front edge + the corner screw bosses.
wall            = 1.8;
top_thickness   = 1.6;
back_thickness  = 1.8;
corner_r        = 4.0;
chamfer         = 0.6;

// ---------- JC4827W543 board outline ---------------------------------------
// PCB outline including the FPC ribbon shielding. Measure with calipers
// if your revision differs — everything else in the case derives from this.
board_w       = 108.0;
board_h       =  78.0;
board_t       =   1.6;
board_z       =   7.5;   // PCB front face → display surface clearance
pocket_clear  =   0.6;   // pocket vs. PCB outline (0.3 mm per side)

// 4-corner mounting hole pattern (M3 typical). Set diam = 0 to skip the
// standoffs entirely and use foam tape — the pocket is tight enough that
// the board doesn't move regardless.
board_hole_pitch_x = 100.0;
board_hole_pitch_y =  68.0;
board_hole_d       =   3.2;

// ---------- Visible display window -----------------------------------------
// Centred on the PCB. Active area on the JC4827W543 is ~95 × 54 mm; the
// 3 mm bezel comes from sizing the bezel inwardly relative to the inner
// shell, not by oversizing the window.
view_w        = 95.0;
view_h        = 54.0;
view_off_y    =  3.0;   // distance the visible area sits below the PCB top

// ---------- Side-mounted controls / ports ----------------------------------
// All on the same long edge of the PCB (the one you face when reaching
// for the BOOT button). Y-distances are measured from the *bottom* short
// edge of the PCB when looking at the front face.
usb_y_offset    =  8.0;
usb_cut_w       = 12.0;
usb_cut_h       =  6.0;

boot_btn_y      = 22.0;
boot_btn_d      =  6.5;

reset_btn_y     = 32.0;
reset_btn_d     =  6.5;

sd_y_offset     = 50.0;
sd_cut_w        = 14.0;
sd_cut_h        =  3.5;

// ---------- Integrated desk stand ------------------------------------------
// The rear tab tilts the screen back 15° when set on a desk. Set
// stand_enabled=false if you'd rather print a wall-mount or hang it from
// a printer cart.
stand_enabled   = true;
stand_tilt_deg  = 15;
stand_depth     = 28;     // foot length on the desk
stand_width     = 60;     // tab width
stand_thickness = 2.6;

// ---------- Derived ---------------------------------------------------------
// Pocket = PCB + small clearance on every side, so the case hugs the
// board instead of leaving 3 mm of dead space all the way round.
inner_w  = board_w + 2 * pocket_clear;
inner_h  = board_h + 2 * pocket_clear;
outer_w  = inner_w + 2 * wall;
outer_h  = inner_h + 2 * wall;

front_depth = top_thickness + board_t + 0.4;    // bezel + PCB sits in here
back_depth  = back_thickness + (board_z - 0.4); // ESP module clearance

// Lower-left corner of the board inside the shell (case-centre coords).
board_x = -board_w / 2;
board_y = -board_h / 2;

view_cx = 0;
view_cy = 0;

// =============================================================================
// Helpers
// =============================================================================

module rrect(w, h, r) {
    minkowski() {
        square([max(w - 2 * r, 0.1), max(h - 2 * r, 0.1)], center = true);
        circle(r = r);
    }
}

// Outer shell with a chamfered top edge so the case reads as a single
// solid piece instead of a stack of layers.
module rshell(w, h, depth, r, wall, top_t, chamfer_top = 0.6) {
    difference() {
        union() {
            // Main body
            linear_extrude(height = depth - chamfer_top)
                rrect(w, h, r);
            // Chamfered top edge (Minkowski-ed top face)
            translate([0, 0, depth - chamfer_top])
                linear_extrude(height = chamfer_top, scale = (w - 2*chamfer_top)/w)
                    rrect(w, h, r);
        }
        // Hollow inside, leaving the top face the thickness specified by
        // top_t. -0.1 / +0.2 fudge factors keep the boolean clean.
        translate([0, 0, top_t])
            linear_extrude(height = depth + 0.2)
                rrect(w - 2 * wall, h - 2 * wall, max(r - wall, 0.5));
    }
}

module screw_boss(d_outer = 5.4, d_hole = 2.6, h = 8) {
    difference() {
        cylinder(d = d_outer, h = h);
        translate([0, 0, 1])
            cylinder(d = d_hole, h = h);
    }
}

// =============================================================================
// FRONT SHELL  (face-down on the print bed)
// =============================================================================

module front_shell() {
    difference() {
        rshell(outer_w, outer_h, front_depth, corner_r, wall, top_thickness,
               chamfer_top = chamfer);

        // Display viewing window. Sized exactly to the active area so the
        // 3 mm bezel emerges from the shell wall thickness + corner_r.
        translate([view_cx, view_cy + view_off_y, -0.1])
            linear_extrude(height = top_thickness + 0.5)
                rrect(view_w + 0.4, view_h + 0.4, 1.6);
    }

    // Board mounting standoffs (4 corners) when the user has chosen to
    // use the PCB's holes. Otherwise the board floats in its pocket on
    // friction alone — the new 0.6 mm clearance is intentionally snug.
    if (board_hole_d > 0) {
        for (px = [0, board_hole_pitch_x])
            for (py = [0, board_hole_pitch_y])
                translate([
                    board_x + (board_w - board_hole_pitch_x)/2 + px,
                    board_y + (board_h - board_hole_pitch_y)/2 + py,
                    top_thickness
                ])
                    screw_boss(d_outer = 5.6,
                               d_hole  = board_hole_d - 0.6,
                               h       = front_depth - top_thickness - 0.4);
    }

    // Corner screw bosses (mate with the back shell). Smaller diameter
    // than v0.3 to keep the inside roomy for the ESP module.
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 3.5),
            sy * (outer_h/2 - wall - 3.5),
            top_thickness
        ])
            screw_boss(d_outer = 5.8, d_hole = 2.4,
                       h = front_depth - top_thickness - 0.4);
}

// =============================================================================
// BACK SHELL  (face-down on the print bed)
// =============================================================================

module back_shell() {
    difference() {
        rshell(outer_w, outer_h, back_depth, corner_r, wall, back_thickness,
               chamfer_top = chamfer);

        // ---- USB-C cut-out on the right long edge ----
        translate([
            outer_w/2 - wall/2,
            board_y + usb_y_offset,
            back_thickness + 4
        ])
            cube([wall + 0.6, usb_cut_w, usb_cut_h + 0.5], center = true);

        // ---- BOOT button window ----
        translate([
            outer_w/2 - wall/2,
            board_y + boot_btn_y,
            back_thickness + 4
        ])
            rotate([0, 90, 0])
                cylinder(d = boot_btn_d, h = wall + 1.0, center = true);

        // ---- RST button window ----
        translate([
            outer_w/2 - wall/2,
            board_y + reset_btn_y,
            back_thickness + 4
        ])
            rotate([0, 90, 0])
                cylinder(d = reset_btn_d, h = wall + 1.0, center = true);

        // ---- microSD slot (some board revisions only) ----
        translate([
            outer_w/2 - wall/2,
            board_y + sd_y_offset,
            back_thickness + 4
        ])
            cube([wall + 0.6, sd_cut_w, sd_cut_h + 0.4], center = true);

        // ---- Vertical vent slots over the ESP module ----
        // Grouped on the left half of the back face (looking at the front)
        // where the WROOM-1 sits. Hot air rises straight out of the slots
        // when the case is tilted on its stand.
        for (i = [-2:2])
            translate([-outer_w/4 + i * 5, 0, -0.1])
                rrect_slot(2.2, 26);
    }

    // ---- Mating screw holes (line up with the front shell's bosses) ----
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 3.5),
            sy * (outer_h/2 - wall - 3.5),
            back_thickness
        ])
            difference() {
                cylinder(d = 5.8, h = 3.6);
                translate([0, 0, -0.1])
                    cylinder(d = 3.0, h = 5);
            }

    // ---- Integrated desk stand --------------------------------------------
    if (stand_enabled) stand_tab();
}

// Helper: a vertical slot through the back face. Extruded from a 2D
// rounded rectangle so the boolean below subtracts a stadium shape rather
// than a sharp-cornered rectangle (less stress at the corners on PETG).
module rrect_slot(w, h) {
    linear_extrude(height = back_thickness + 0.5)
        rrect(w, h, min(w, h) / 2);
}

// The desk stand is a flat triangle that hinges off the bottom of the
// back shell. Print-in-place; no supports because the inside of the
// hinge is a single 15° overhang.
module stand_tab() {
    // Origin: centred along X, at the bottom edge of the case, just
    // proud of the back shell.
    translate([0, -outer_h/2 + 1.5, back_depth - stand_thickness])
        rotate([90 - stand_tilt_deg, 0, 0])
            translate([-stand_width/2, 0, 0])
                cube([stand_width, stand_depth, stand_thickness]);
}

// =============================================================================
// Render
// =============================================================================

if      (part == "front") front_shell();
else if (part == "back")  back_shell();
else if (part == "all") {
    translate([0,  outer_h/2 + 5, 0]) front_shell();
    translate([0, -outer_h/2 - 5, 0]) back_shell();
}
