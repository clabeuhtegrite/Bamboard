// ============================================================================
// TFT_eSPI configuration for Bamboard
// Target: ESP32-S3 + 4" ILI9488 480x320 SPI display
// ============================================================================
//
// This file overrides the default TFT_eSPI configuration. It is auto-included
// by platformio.ini via `-include "User_Setup.h"`.
//
// Display pinout (matches case/bamboard.scad mounting plan and
// hardware/wiring.md):
//
//     ESP32-S3 GPIO    ILI9488 pin
//     -------------    -----------
//     11               SDA / MOSI
//     13               SDO / MISO  (optional, only if reading from display)
//     12               SCK
//     10               CS
//     14               DC
//      9               RST
//     21               LED / BL (backlight)
//
// ============================================================================

#pragma once

#define USER_SETUP_INFO "Bamboard - ILI9488 4inch SPI"

// --- Driver ---
#define ILI9488_DRIVER

// --- Colour order ---
// Some 4" panels are BGR. If the colours look wrong (red/blue swapped),
// uncomment the next line instead of the default RGB order.
#define TFT_RGB_ORDER TFT_BGR

// --- Pin mapping (ESP32-S3) ---
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   14
#define TFT_RST   9
#define TFT_BL   21    // Backlight, driven via LEDC PWM in firmware

#define TFT_BACKLIGHT_ON HIGH

// --- Fonts loaded into flash ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// --- SPI frequency ---
// ILI9488 spec is 20 MHz typ., 27 MHz max. ESP32-S3 happily drives 40 MHz
// with short wires, but 27 MHz is the conservative choice for a Dupont jumper
// build. Increase carefully if the panel garbles after long ribbon cable runs.
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000

// --- Use HSPI/VSPI ---
// ESP32-S3 doesn't expose VSPI/HSPI names; default GPSPI is fine.
