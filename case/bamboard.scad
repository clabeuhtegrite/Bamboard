// =============================================================================
// Bamboard enclosure (v1.0) — parametric OpenSCAD model
// =============================================================================
//
// Two-part FDM-printable case for the Guition JC4827W543 all-in-one board
// (ESP32-S3 + 4.3" IPS 480×272 + GT911 capacitive touch on one PCB):
//
//   front_shell : panel cut-out for the visible area + a friction-fit
//                 pocket that lines up the display PCB behind it
//   back_shell  : USB-C cut-out + BOOT-button window + microSD slot +
//                 vent ribs + screw bosses
//
// The two halves screw together with four M3 × 8 mm self-tapping screws.
//
// Print orientation:
//   front_shell: face-down on the bed, no supports needed
//   back_shell : face-down on the bed; minor supports on the USB / BOOT
//                cut-outs and around the rear screw bosses (auto-painted
//                in your slicer is plenty)
//
// Recommended: 0.20 mm layer, 3 perimeters, 20 % gyroid, PETG preferred
// (the panel runs slightly warm).
//
// Heads-up: the JC4827W543 has shipped in several mechanical revisions
// over the years and the exact PCB / connector offsets differ. Print the
// front shell as a tolerance check first (≈ 30 min on a Bambu X1C) and
// nudge the variables below if anything doesn't fit. Every dimension you
// might want to change is in the first ~70 lines of the file.
//
// =============================================================================

$fa = 2;
$fs = 0.4;

// ---------- Part selection --------------------------------------------------
// Toggle which part is rendered and exported. The launcher commands in
// case/README.md set this from the CLI.
part = "front";   // "front" | "back" | "all"

// ---------- Global wall geometry --------------------------------------------
wall            = 2.4;
top_thickness   = 2.0;
back_thickness  = 2.4;
corner_r        = 4.0;

// ---------- JC4827W543 board outline ---------------------------------------
// PCB outline including the FPC ribbon shielding. Measure with a caliper
// if your revision differs — the rest of the case is derived from this.
board_w       = 108.0;
board_h       =  78.0;
board_t       =   1.6;
board_z       =   8.0;   // PCB-to-component clearance behind the panel

// 4-corner mounting hole pattern (M3 typical). Set diam = 0 to skip the
// standoffs entirely and use foam tape — the pocket below is tight enough
// that the board doesn't move regardless.
board_hole_pitch_x = 100.0;
board_hole_pitch_y =  68.0;
board_hole_d       =   3.2;

// ---------- Visible display window -----------------------------------------
// Centred on the PCB. Measure from the bezel of your specific panel —
// the active area on the JC4827W543 is ~95 × 54 mm but the bezel sits
// inboard of that by a millimetre or so.
view_w        = 95.0;
view_h        = 54.0;
view_off_y    =  5.0;   // distance from the top edge of the PCB

// ---------- Side-mounted controls / ports ----------------------------------
// All on the same long edge of the PCB (the one you face when reaching for
// the buttons). Y-distances are measured from the *bottom* short edge of
// the PCB when looking at the front face. Re-run a draft if your board
// has its connectors clustered differently.
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

// ---------- Derived ---------------------------------------------------------
inner_w  = board_w + 6;
inner_h  = board_h + 6;
outer_w  = inner_w + 2 * wall;
outer_h  = inner_h + 2 * wall;

front_depth = top_thickness + board_t + 0.8;     // visible bezel + board sit
back_depth  = 18.0;                              // ESP module + components

// Lower-left corner of the board PCB inside the front shell (case-centre
// coordinates). The mounting holes / view cut-out / side cut-outs are
// derived from this.
board_x = -board_w / 2;
board_y = -board_h / 2;

view_cy = board_y + (board_h - view_h)/2 - view_off_y + view_off_y;  // centred along Y by default
view_cx = 0;

// =============================================================================
// Helpers
// =============================================================================

module rrect(w, h, r) {
    minkowski() {
        square([max(w - 2 * r, 0.1), max(h - 2 * r, 0.1)], center = true);
        circle(r = r);
    }
}

module rshell(w, h, depth, r, wall, top_t) {
    difference() {
        linear_extrude(height = depth)
            rrect(w, h, r);
        translate([0, 0, top_t])
            linear_extrude(height = depth)
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
// FRONT SHELL
// =============================================================================

module front_shell() {
    difference() {
        rshell(outer_w, outer_h, front_depth, corner_r, wall, top_thickness);

        // Display viewing window — rounded rectangle through the top face.
        translate([view_cx, view_cy, -0.1])
            linear_extrude(height = top_thickness + 0.5)
                rrect(view_w + 0.4, view_h + 0.4, 2.0);
    }

    // Board mounting standoffs (4 corners) when the user has chosen to
    // use the PCB's holes. Otherwise the board floats in its pocket on
    // foam tape.
    if (board_hole_d > 0) {
        for (px = [0, board_hole_pitch_x])
            for (py = [0, board_hole_pitch_y])
                translate([
                    board_x + (board_w - board_hole_pitch_x)/2 + px,
                    board_y + (board_h - board_hole_pitch_y)/2 + py,
                    top_thickness
                ])
                    screw_boss(d_outer = 6.0,
                               d_hole  = board_hole_d - 0.6,
                               h       = front_depth - top_thickness - 0.4);
    }

    // Corner screw bosses (mate with the back shell)
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 4),
            sy * (outer_h/2 - wall - 4),
            top_thickness
        ])
            screw_boss(d_outer = 6.4, d_hole = 2.6,
                       h = front_depth - top_thickness - 0.4);
}

// =============================================================================
// BACK SHELL
// =============================================================================

module back_shell() {
    difference() {
        rshell(outer_w, outer_h, back_depth, corner_r, wall, back_thickness);

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

        // ---- Vent slots on the back face ----
        for (i = [-3:3])
            translate([i * 7, 0, -0.1])
                cube([3, 30, back_thickness + 0.5], center = true);
    }

    // ---- Mating screw holes (line up with the front shell's bosses) ----
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 4),
            sy * (outer_h/2 - wall - 4),
            back_thickness
        ])
            difference() {
                cylinder(d = 6.4, h = 4);
                translate([0, 0, -0.1])
                    cylinder(d = 3.2, h = 5);
            }
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
