# Flashing the firmware

- [Prerequisites](#prerequisites)
- [First-time build](#first-time-build)
- [Flash (first install over USB)](#flash-first-install-over-usb)
- [First boot](#first-boot)
- [Re-configuring later](#re-configuring-later)
- [OTA updates](#ota-updates)
- [Troubleshooting](#troubleshooting)

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

## Flash (first install over USB)

### One-click flash scripts (recommended)

Plug the ESP32-S3 in via USB-C, then run the launcher for your OS:

| OS      | How to run                              |
|---------|------------------------------------------|
| Windows | Double-click `scripts/flash-windows.bat` |
| macOS   | Double-click `scripts/flash-mac.command` |
| Linux   | `scripts/flash-linux.sh`                 |

What the script does:

- lists USB serial ports via `pio device list`, scores them by VID:PID
  (Espressif native USB > CP210x > CH340 > FTDI), and **auto-picks**
  when only one credible match exists;
- asks for confirmation before flashing — pass `-y` to skip the prompt
  when scripting;
- on Linux, prints the `dialout` group hint when the user lacks
  serial-port permissions.

Useful flags (forwarded by every launcher):

- `--port COM5` — bypass detection.
- `-y`, `--yes` — auto-confirm when a single port was detected.
- `--erase` — full chip erase before flashing (wipes Wi-Fi creds,
  Bambuddy URL, API key — useful when reusing a board).
- `--monitor` — open the serial monitor after a successful flash so
  you can watch the boot log.
- `--build-only` — compile only, don't upload.

### Manual fallback

If you'd rather not run the wrapper, the underlying PlatformIO commands
are:

```bash
cd firmware/
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

### One-click update scripts (recommended)

For a smoother experience, the repo ships per-OS wrappers under
`scripts/` that compile, discover the device, and upload in one step:

| OS      | How to run                              |
|---------|------------------------------------------|
| Windows | Double-click `scripts/update-windows.bat` (or run it from `cmd`) |
| macOS   | Double-click `scripts/update-mac.command` in Finder, or `scripts/update-mac.command` from a terminal |
| Linux   | `scripts/update-linux.sh`                |

The scripts:

- try `bamboard.local` via mDNS first, fall back to a one-time IP
  prompt and **cache the answer** so the next run doesn't ask;
- read the OTA password from `$BAMBOARD_OTA_PASSWORD` if set,
  otherwise use the firmware default (`bamboard`);
- locate `pio` on PATH or under `~/.platformio/penv/`, so you don't
  need PlatformIO on PATH for the script to find it;
- stream PlatformIO's compile and upload output through, then print a
  one-line success / failure summary.

Useful flags (forwarded by every launcher):

- `--reset` — clear the cached host and re-discover.
- `--host 192.168.1.42` — bypass discovery for one run.
- `--password mysecret` — bypass the env var / default for one run.
- `--build-only` — compile without uploading.

### Changing the OTA password

The default password is `bamboard`. To bake in a stronger one, edit
`platformio.ini` and add to `build_flags`:

```ini
build_flags =
    ...
    -DOTA_PASSWORD_OVERRIDE='"my-secret"'
```

then flash once over USB so the new firmware (with the new password)
gets installed. Subsequent OTA pushes must use the same value in
`--auth=` — or simpler, set `BAMBOARD_OTA_PASSWORD=my-secret` in your
shell and the update scripts pick it up automatically.

### What you see on the device

During the upload the device shows a progress bar; the rest of the UI is
suppressed until the new firmware boots. On failure the bottom banner
turns red and prints the reason (wrong password, connection lost,
flash full, …).

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
