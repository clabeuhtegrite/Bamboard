# Flashing the firmware

- [Flash from your browser (recommended)](#flash-from-your-browser-recommended)
- [Manual flash with esptool](#manual-flash-with-esptool)
- [Building from source](#building-from-source)
- [First boot](#first-boot)
- [Re-configuring later](#re-configuring-later)
- [Updates (over-the-air, from GitHub)](#updates-over-the-air-from-github)
- [Troubleshooting](#troubleshooting)

## Flash from your browser (recommended)

The whole first install happens on a web page — no toolchain, no CLI.

1. Open **<https://clabeuhtegrite.github.io/Bamboard/>** in **Chrome, Edge or
   Opera on a desktop**. (Web Serial — the browser API that talks to the
   device — is **not** available in Firefox, Safari or any mobile browser.)
2. Plug the board into a USB-C **data** cable (the slim white cables bundled
   with phone chargers are often charge-only).
3. Click **Connect** and pick the serial port that appears.
4. If no port shows up, or the connection drops, put the board in download
   mode: hold **BOOT**, tap **RST**, release **BOOT**, then click Connect
   again.
5. The page flashes the full firmware image and the device reboots into the
   [first-boot](#first-boot) Wi-Fi + Bambuddy setup.

The page always installs the firmware from the **latest GitHub Release** —
it's rebuilt and redeployed automatically every time a release is cut.

## Manual flash with esptool

For Firefox / Safari / a Linux box without a Chromium browser, or if you just
prefer the command line, download **`bamboard-factory.bin`** from the
[latest release](https://github.com/clabeuhtegrite/Bamboard/releases/latest)
and write it at offset `0x0`:

```bash
pip install esptool
esptool.py --chip esp32s3 write_flash 0x0 bamboard-factory.bin
```

`bamboard-factory.bin` is the full image (bootloader + partition table +
boot_app0 + app). To wipe a previously-configured board first (clears Wi-Fi
creds, Bambuddy URL and API key), run `esptool.py --chip esp32s3 erase_flash`
before writing.

## Building from source

Only needed if you want to modify the firmware. The browser/esptool flows
above use the prebuilt release binary.

```bash
cd firmware/
pio run            # compile + download libraries (~5 min the first time)
pio run -t upload  # flash over USB via PlatformIO
pio device monitor # 115200 baud
```

You need [PlatformIO Core](https://platformio.org/install/cli) or the VS Code
PlatformIO extension. The build pulls in LovyanGFX, LVGL, ArduinoJson,
WiFiManager and the WebSockets client.

## First boot

On a fresh device with no saved settings:

1. The screen displays the **Bamboard setup** instructions.
2. The device exposes a Wi-Fi AP called `Bamboard-setup` (password
   `bamboard`).
3. Connect any phone/laptop to it. Most OSes will open a captive portal
   automatically. If not, browse to `http://192.168.4.1`.
4. Pick your home Wi-Fi network, enter the password, **and** fill in the
   custom fields:
   - **Bambuddy URL** — e.g. `http://192.168.1.42:8000`
   - **Bambuddy API key** — created in your Bambuddy install at Settings
     → API Keys → Create API Key, with at least `printers:read` and
     `statistics:read` permissions
   - **Timezone** — a POSIX TZ string (the portal shows ready-to-paste
     examples); sets the clock for the daily reboot. Defaults to Europe/Paris.
   - **Daily reboot hour** — `0`–`23` local time, or leave blank to turn the
     daily auto-reboot off.
   - **Language** — interface language: `en`, `es`, `fr`, `pt` or `de`.
5. The device reboots and connects.

## Re-configuring later

You don't need to reflash to change the Bambuddy URL, the API key, or the
Wi-Fi network. Hold the side **BOOT** button while the device boots — this
wipes the saved settings and re-opens the captive portal. (The Settings
screen also has a two-tap **Factory reset** button that does the same.)

## Updates (over-the-air, from GitHub)

After the first flash you never need a cable again. On every boot the device
checks this repo's **latest GitHub Release** and, if it carries a newer
firmware than the one running, downloads and flashes it before the rest of the
UI comes up. If the device is offline, GitHub is unreachable, or it's already
on the latest version, boot just continues.

The device also **reboots itself once a day at local midnight** so this check
runs unattended — a device that's never manually power-cycled still picks up
new releases overnight. The hour and timezone are configurable in the
`schedule` namespace in `config.h` (see [configuration.md](configuration.md)).

How it works:

- The device fetches
  `https://github.com/clabeuhtegrite/Bamboard/releases/latest/download/manifest.json`
  — a tiny file (`{version, tag, url, sha256, md5, size}`) attached to every
  release. The `latest/download/...` URL always resolves to the newest
  release, so nothing on the device has to change between versions.
- It compares `manifest.version` against its own build version (shown at
  the bottom of the Settings screen). If the release is newer, it
  downloads the `firmware.bin` asset (the app image, not the factory image),
  flashes it into the spare OTA partition, and reboots into it. The image is
  verified against the manifest's `md5` at flash time — a release without a
  valid `md5` is refused rather than flashed unverified.
- Dev builds — compiled locally with no version tag — skip the check, so a
  USB-flashed work-in-progress isn't immediately pulled back to the
  published release. (You can also force-disable the check with
  `-DBAMBOARD_OTA_AUTOCHECK=0` in `build_flags`.)

### Cutting a release

Releases are produced automatically by GitHub Actions — you never build or
upload binaries by hand:

```bash
git tag v0.5.1
git push origin v0.5.1
```

The [`release` workflow](../.github/workflows/release.yml) compiles the
firmware with that version baked in, builds the OTA `firmware.bin` and the
`bamboard-factory.bin` full image, creates the GitHub Release with both plus
`manifest.json`, and redeploys the [web installer](../web/) to GitHub Pages.
Use plain `vMAJOR.MINOR.PATCH` tags. Within a few minutes every device picks
up the new version on its next boot, and the web installer offers it too.

### What you see on the device

While an update downloads, the device shows a full-screen progress bar and
the rest of the UI is suppressed until the new firmware boots. If the
download fails it briefly shows the reason, then continues on the current
firmware and retries on the next boot.

## Troubleshooting

- **Browser shows "this browser can't talk to USB serial devices"** — you're
  on Firefox, Safari or mobile. Use Chrome / Edge / Opera on a desktop, or
  flash manually with [esptool](#manual-flash-with-esptool).
- **No serial port appears** — the cable may be charge-only (try another), or
  the board isn't in download mode: hold **BOOT**, tap **RST**, release
  **BOOT**, retry. On Linux, add yourself to the `dialout` group
  (`sudo usermod -aG dialout $USER`, then re-login) for port access.
- **Screen stays dark after flashing** — the backlight (GPIO 2, PWM) ramps up
  only once the first LVGL frame is drawn. A boot loop before that leaves it
  dark; check the serial monitor at 115200 baud. The display + touch pin map
  lives at the top of [`firmware/src/hw/display.cpp`](../firmware/src/hw/display.cpp).
- **Wrong colours / banding** — you likely have a different panel revision.
  The pin map targets the JC4827W543**C**; resistive (`R`) revisions and the
  800×480 sibling (JC8048W550) use a different mapping (see the notes in
  `display.cpp`).
- **HTTP 401 from Bambuddy** — the API key is missing the `printers:read`
  permission, or has been revoked.
- **Empty printer list** — verify in the Bambuddy web UI that printers show up
  there. Bamboard is read-only as far as adding printers goes; add them
  upstream in Bambuddy first.
