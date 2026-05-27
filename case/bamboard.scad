// =============================================================================
// Bamboard enclosure - parametric OpenSCAD model
// =============================================================================
//
// A two-part FDM-printable case for the Bamboard project:
//   - front_shell : holds the 4" ILI9488 display and the three buttons
//   - back_shell  : holds the ESP32-S3 DevKitC, exposes the USB-C port,
//                   includes screw bosses and a small light pipe for the
//                   WS2812 status LED.
//
// The two halves clip together with four M2.5 self-tapping screws.
//
// Render either part by toggling `part` below, then F6 → Export STL.
//
// Variables are at the top of the file so you can adapt the model to a
// slightly different display panel without touching geometry. The defaults
// match a typical 4.0" SPI ILI9488 sold on AliExpress (≈ 99×60 mm board).
//
// Print orientation:
//   - front_shell: face-down on the bed, no supports needed
//   - back_shell : face-down on the bed, supports on overhangs > 50°
//
// Recommended: 0.2 mm layer, 3 perimeters, 20% infill, PETG or PLA.
//
// =============================================================================

$fa = 2;
$fs = 0.4;

// ---------- Part selection ---------------------------------------------------
part = "front";   // "front" | "back" | "both" | "buttoncap"

// ---------- Global ----------------------------------------------------------
wall            = 2.4;
top_thickness   = 2.0;
back_thickness  = 2.4;
corner_r        = 4.0;

// ---------- Display panel ---------------------------------------------------
// PCB outline (the board, not the visible area)
disp_pcb_w      = 99.0;
disp_pcb_h      = 60.0;
disp_pcb_t      = 1.6;

// Visible viewing area
disp_view_w     = 76.5;
disp_view_h     = 56.5;
disp_view_off_x = 11.0;   // from the left edge of the PCB
disp_view_off_y = 1.5;

// Standoff hole pattern (4 corners of the PCB)
disp_hole_pitch_x = 92.5;
disp_hole_pitch_y = 53.0;
disp_hole_d       = 2.4;

// ---------- ESP32-S3 DevKitC-1 ---------------------------------------------
esp_w    = 25.4;
esp_h    = 51.5;
esp_t    = 1.6;
esp_usb_offset_y = 3.5;   // USB-C centre, from the top edge of the PCB
esp_clearance_t  = 8.0;   // headroom above the PCB for components

// ---------- Buttons ---------------------------------------------------------
btn_d           = 6.6;      // through-hole for the cap; tactile sw body is 6 mm
btn_spacing     = 24.0;     // centre-to-centre
btn_offset_x_from_disp = 0; // centred under the display
btn_offset_y    = 18.0;     // distance below the visible area

// ---------- WS2812 light pipe ----------------------------------------------
led_d           = 4.2;
led_offset_y    = 8.0;      // from the top of the device

// ---------- Derived ---------------------------------------------------------
inner_w = disp_pcb_w + 8;
inner_h = disp_pcb_h + 24;     // extra space below for buttons + LED
outer_w = inner_w + 2 * wall;
outer_h = inner_h + 2 * wall;

front_depth = 8.5;     // shell depth for the front half
back_depth  = 14.0;    // shell depth for the back half (houses MCU)

// ============================================================================
// Helpers
// ============================================================================

module rrect(w, h, r) {
    minkowski() {
        square([w - 2 * r, h - 2 * r], center = true);
        circle(r = r);
    }
}

module rshell(w, h, depth, r, wall, top_t) {
    // Hollow rounded shell, closed on top.
    difference() {
        // Outer solid
        linear_extrude(height = depth)
            rrect(w, h, r);
        // Inner cavity (leaves top closed by top_t)
        translate([0, 0, top_t])
            linear_extrude(height = depth)
                rrect(w - 2 * wall, h - 2 * wall, max(r - wall, 0.5));
    }
}

module screw_boss(d_outer = 5.4, d_hole = 2.1, h = 8) {
    difference() {
        cylinder(d = d_outer, h = h);
        translate([0, 0, 1])
            cylinder(d = d_hole, h = h);
    }
}

module clip_pin(d = 3.2, h = 4) {
    cylinder(d = d, h = h);
}

// ============================================================================
// FRONT SHELL
// ============================================================================

module front_shell() {
    difference() {
        rshell(outer_w, outer_h, front_depth, corner_r, wall, top_thickness);

        // Display viewing window
        translate([
            -outer_w/2 + wall + (outer_w - 2*wall - disp_pcb_w)/2 + disp_view_off_x + disp_view_w/2,
            outer_h/2 - wall - disp_view_off_y - disp_view_h/2 - 4,
            -0.1
        ])
            linear_extrude(height = top_thickness + 0.5)
                rrect(disp_view_w + 0.6, disp_view_h + 0.6, 1.5);

        // Button holes (3 across)
        for (i = [-1, 0, 1]) {
            translate([
                i * btn_spacing + btn_offset_x_from_disp,
                outer_h/2 - wall - disp_view_off_y - disp_view_h - btn_offset_y,
                -0.1
            ])
                cylinder(d = btn_d, h = top_thickness + 0.5);
        }

        // Status LED light pipe hole
        translate([
            outer_w/2 - wall - led_offset_y - led_d/2,
            outer_h/2 - wall - led_offset_y,
            -0.1
        ])
            cylinder(d = led_d, h = top_thickness + 0.5);
    }

    // Display mounting standoffs (4 corners, M2 self-tap)
    disp_origin_x = -outer_w/2 + wall + (outer_w - 2*wall - disp_pcb_w)/2;
    disp_origin_y = outer_h/2 - wall - 4 - disp_pcb_h;
    for (px = [0, disp_hole_pitch_x])
        for (py = [0, disp_hole_pitch_y])
            translate([
                disp_origin_x + (disp_pcb_w - disp_hole_pitch_x)/2 + px,
                disp_origin_y + (disp_pcb_h - disp_hole_pitch_y)/2 + py,
                top_thickness
            ])
                screw_boss(d_outer = 4.8, d_hole = 1.7, h = 4.0);

    // Corner screw bosses (mate with the back shell)
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 4),
            sy * (outer_h/2 - wall - 4),
            top_thickness
        ])
            screw_boss(d_outer = 6.0, d_hole = 2.1, h = front_depth - top_thickness - 0.4);
}

// ============================================================================
// BACK SHELL
// ============================================================================

module back_shell() {
    difference() {
        rshell(outer_w, outer_h, back_depth, corner_r, wall, back_thickness);

        // USB-C cutout (bottom edge of the back shell)
        // ESP32-S3 DevKitC USB-C is roughly 9 × 4 mm.
        translate([
            -outer_w/2 + wall + 4,
            -outer_h/2 + wall - 0.1,
            back_thickness + 4
        ])
            cube([10, wall + 0.5, 5]);

        // Vents (small slots on the back face)
        for (i = [-3:3])
            translate([i * 5, -8, -0.1])
                cube([2, 30, back_thickness + 0.5], center = true);
    }

    // ESP32 holder rails: two simple vertical ribs that pinch the DevKitC
    rail_h = 10;
    rail_x = esp_w/2 + 0.4;
    translate([0, -outer_h/2 + wall + 4, back_thickness]) {
        translate([-rail_x, 0, 0]) cube([2, esp_h, rail_h]);
        translate([ rail_x - 2, 0, 0]) cube([2, esp_h, rail_h]);
    }

    // Mating screw holes (line up with the front shell's bosses)
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([
            sx * (outer_w/2 - wall - 4),
            sy * (outer_h/2 - wall - 4),
            back_thickness
        ])
            difference() {
                cylinder(d = 6.0, h = 4);
                translate([0, 0, -0.1])
                    cylinder(d = 2.4, h = 5);
            }
}

// ============================================================================
// Optional: nicer button caps (print 3×)
// ============================================================================

module buttoncap() {
    h = 4.5;
    difference() {
        union() {
            cylinder(d = btn_d - 0.4, h = h);
            translate([0, 0, h])
                cylinder(d1 = btn_d - 0.4, d2 = btn_d - 2.0, h = 1.5);
        }
        translate([0, 0, -0.1])
            cylinder(d = 3.6, h = 2.5);
    }
}

// ============================================================================
// Render
// ============================================================================

if (part == "front")        front_shell();
else if (part == "back")    back_shell();
else if (part == "buttoncap") buttoncap();
else if (part == "both") {
    translate([0,  outer_h/2 + 4, 0]) front_shell();
    translate([0, -outer_h/2 - 4, 0]) back_shell();
}
