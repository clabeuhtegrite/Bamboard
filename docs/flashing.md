# Flashing the firmware

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) or the VS Code
  PlatformIO extension.
- A USB-C cable that supports data (most do, but the slim white ones
  bundled with phone chargers often don't).

## First-time build

```bash
cd firmware/
pio run                # compiles and downloads all libraries
```

The first build pulls in TFT_eSPI, LVGL, ArduinoJson, WiFiManager and
FastLED, then compiles them. Expect ~5 minutes on a modern laptop.

## Flash

Plug the ESP32-S3 in via USB-C. On most boards no button dance is needed
— PlatformIO triggers ROM bootloader via DTR/RTS.

```bash
pio run -t upload
pio device monitor    # 115200 baud
```

If your particular DevKitC has a stubborn USB port, hold **BOOT**, tap
**RESET**, release **BOOT**, then re-run `pio run -t upload`.

## First boot

On a fresh device with no saved settings:

1. The screen displays the **Bamboard setup** instructions.
2. The device exposes a Wi-Fi AP called `Bamboard-setup` (password
   `bamboard`).
3. Connect any phone/laptop to it. Most OSes will open a captive portal
   automatically. If not, browse to `http://192.168.4.1`.
4. Pick your home Wi-Fi network, enter the password, **and** fill in the
   two custom fields:
   - **Bambuddy URL** — e.g. `http://192.168.1.42:8000`
   - **Bambuddy API key** — created in your Bambuddy install at Settings
     → API Keys → Create API Key, with at least `printers:read` and
     `statistics:read` permissions
5. The device reboots and connects.

## Re-configuring later

You don't need to reflash to change the Bambuddy URL, the API key, or the
Wi-Fi network. Hold the **PREV** button while the device boots — this
wipes the saved settings and re-opens the captive portal.

## OTA updates

After the first flash, the ESP32-S3 advertises itself for ArduinoOTA on
the LAN as `bamboard`. From PlatformIO you can push firmware over the
network with:

```bash
pio run -t upload --upload-port bamboard.local --upload-flags --auth=bamboard
```

The default OTA password is `bamboard`. If you'd rather not have your
LAN neighbours be able to reflash the device, override it at build time:

```bash
pio run -t upload \
    --upload-port bamboard.local \
    --upload-flags --auth=my-secret \
    -e esp32-s3-devkitc-1 \
    -DOTA_PASSWORD_OVERRIDE='"my-secret"'
```

or set `build_flags += -DOTA_PASSWORD_OVERRIDE='"my-secret"'` in
`platformio.ini` and flash once over USB so the new password is baked
into the firmware. Subsequent OTA pushes must then use the same value
in `--auth`.

During the upload the device shows a progress bar; the rest of the UI is
suppressed until the new firmware boots.

## Troubleshooting

- **Screen is dark** — check `TFT_BL` (GPIO 21) wiring and that the LED
  pin on the panel is powered. The firmware ramps the backlight from 0
  to full once the first LVGL frame is drawn; if you never reach that
  frame the screen stays dark.
- **Wrong colours** — toggle `TFT_RGB_ORDER` in
  `firmware/include/User_Setup.h` between `TFT_BGR` and `TFT_RGB`.
- **HTTP 401 from Bambuddy** — the API key is missing the
  `printers:read` permission, or has been revoked.
- **Empty printer list** — verify in the Bambuddy web UI that printers
  show up there. Bamboard is read-only as far as adding printers goes;
  add them upstream in Bambuddy first.
