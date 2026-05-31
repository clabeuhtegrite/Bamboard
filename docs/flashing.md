# Flashing the firmware

- [Prerequisites](#prerequisites)
- [First-time build](#first-time-build)
- [Flash (first install over USB)](#flash-first-install-over-usb)
- [First boot](#first-boot)
- [Re-configuring later](#re-configuring-later)
- [Updates (over-the-air, from GitHub)](#updates-over-the-air-from-github)
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

The first build pulls in LovyanGFX, LVGL, ArduinoJson, WiFiManager and
the WebSockets client, then compiles them. Expect ~5 minutes on a modern
laptop.

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
Wi-Fi network. Hold the side **BOOT** button while the device boots — this
wipes the saved settings and re-opens the captive portal. (The Settings
screen also has a two-tap **Factory reset** button that does the same.)

## Updates (over-the-air, from GitHub)

After the first USB flash you never need a cable again. On every boot the
device checks this repo's **latest GitHub Release** and, if it carries a
newer firmware than the one running, downloads and flashes it before the
rest of the UI comes up. If the device is offline, GitHub is unreachable,
or it's already on the latest version, boot just continues.

How it works:

- The device fetches
  `https://github.com/clabeuhtegrite/Bamboard/releases/latest/download/manifest.json`
  — a tiny file (`{version, tag, url, sha256, size}`) attached to every
  release. The `latest/download/...` URL always resolves to the newest
  release, so nothing on the device has to change between versions.
- It compares `manifest.version` against its own build version (shown at
  the bottom of the Settings screen). If the release is newer, it
  downloads the `firmware.bin` asset, flashes it into the spare OTA
  partition, and reboots into it.
- Dev builds — compiled locally with no version tag — skip the check, so a
  USB-flashed work-in-progress isn't immediately pulled back to the
  published release. (You can also force-disable the check with
  `-DBAMBOARD_OTA_AUTOCHECK=0` in `build_flags`.)

### Cutting a release

Releases are produced automatically by GitHub Actions — you never build or
upload the binary by hand:

```bash
git tag v1.2.0
git push origin v1.2.0
```

The [`release` workflow](../.github/workflows/release.yml) compiles the
firmware with that version baked in, generates `manifest.json`, and creates
the GitHub Release with both assets attached. Use plain
`vMAJOR.MINOR.PATCH` tags. Within a few minutes every device picks up the
new version on its next boot.

### What you see on the device

While an update downloads, the device shows a full-screen progress bar and
the rest of the UI is suppressed until the new firmware boots. If the
download fails it briefly shows the reason, then continues on the current
firmware and retries on the next boot.

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
