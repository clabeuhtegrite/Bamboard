# Assembly

A quiet hour with a screwdriver, no soldering iron required (assuming you
bought the parts as listed in [`hardware/bom.md`](../hardware/bom.md)).

## Tools

- Phillips PH00 screwdriver
- A pair of side cutters (only to trim Dupont jumpers if you want a tidy build)
- USB-C cable

## Step 1 — Print the case

See [`case/README.md`](../case/README.md). You need:

- 1× front shell
- 1× back shell
- 3× button cap (optional, the bare tactile switch nubs work but feel meh)

## Step 2 — Wire up the display

1. Plug 11 female-female jumpers into the back of the ILI9488 board, in
   this order (label each one with masking tape — future-you will be glad):

   `VCC GND CS RESET DC SDI SCK LED SDO`

2. Plug the other ends into the ESP32-S3 according to
   [`hardware/wiring.md`](../hardware/wiring.md).

## Step 3 — Wire up the buttons and LED

1. Solderless approach: take a small piece of perfboard, push 3 tactile
   switches into it, and wire one leg of each to GND with a single
   shared rail. Run the second leg of each switch to a Dupont jumper
   going to GPIO 4 / 5 / 6.

2. Plug the WS2812 module's three pins (VCC / GND / DIN) to 5V / GND / GPIO 7.

> If you'd rather skip the perfboard, you can dead-bug the buttons
> directly to the inside of the front shell with hot glue — there are
> button cutouts already aligned in the print.

## Step 4 — First mechanical test

1. Drop the display into the front shell, viewing area facing out.
2. Screw it down with 4× M2 × 6 mm screws into the printed standoffs.
3. Insert the buttons through the cutouts and add the button caps.

## Step 5 — Mount the MCU

1. Slide the ESP32-S3 DevKitC into the back shell, between the two
   printed rails, with the USB-C port lined up with the cutout.
2. Route all the jumpers to keep them clear of the screw bosses.

## Step 6 — Flash the firmware

Don't close the case yet — flash first. See [`flashing.md`](flashing.md).

## Step 7 — Close it up

1. Place the back shell over the front, lining up the four corner bosses.
2. Drive 4× M2.5 × 8 mm self-tappers through the back shell into the
   front shell bosses. Snug, don't over-tighten — the bosses are 4 mm
   thick.

You're done. Plug in USB-C — within 10 seconds the screen should light
up and either join your Wi-Fi or open the captive portal.
