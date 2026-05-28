#!/usr/bin/env python3
"""
Bamboard — first-install flash helper.

Walks the user through the very first USB flash so the only thing they
have to do is plug in the device. Concretely:

  1. Lists candidate USB serial ports via ``pio device list`` and scores
     them by VID:PID (Espressif native USB > CP210x > CH340 > FTDI).
  2. Auto-picks when only one credible candidate exists; otherwise shows
     the list and asks the user to choose.
  3. Runs ``pio run -t upload --upload-port <port>`` so PlatformIO can
     drive esptool with the bootloader / partition table / app binary
     in the right order.
  4. Optionally erases the flash first (``--erase``) — handy when reusing
     a board that already has Bamboard settings saved — and / or opens
     the serial monitor after upload (``--monitor``) so the user can see
     the captive-portal AP coming up live.

After the flash succeeds the device reboots into the captive-portal
flow described in docs/flashing.md, which this script just refers to
at the end — there's no point reimplementing Wi-Fi provisioning here.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
import time

from _common import (
    banner,
    fail,
    find_pio,
    info,
    quote_arg,
    require_python,
    stage,
)


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
FIRMWARE  = REPO_ROOT / "firmware"

# Known USB-UART VID:PID combos for ESP32-S3 devkits, in priority order.
# Higher score = more likely to be the Bamboard. We match on hwid which
# pyserial / pio normalize as "USB VID:PID=XXXX:YYYY ...".
USB_SCORES: list[tuple[str, int, str]] = [
    ("303A:1001", 100, "Espressif native USB (DevKitC-1, S3 family)"),
    ("303A:",      90, "Espressif native USB (other S3 board)"),
    ("10C4:EA60",  80, "Silicon Labs CP210x"),
    ("1A86:7523",  70, "QinHeng CH340"),
    ("1A86:55D4",  70, "QinHeng CH343"),
    ("0403:6001",  60, "FTDI FT232"),
]


# ---------- USB port discovery --------------------------------------------

def list_pio_devices(pio: str) -> list[dict]:
    try:
        proc = subprocess.run(
            [pio, "device", "list", "--json-output"],
            capture_output=True, text=True, check=True,
        )
    except subprocess.CalledProcessError as exc:
        fail(f"`pio device list` failed:\n    {exc.stderr.strip() or exc}")
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        fail(f"Could not parse `pio device list` output: {exc}")


def score_device(d: dict) -> tuple[int, str]:
    hwid = (d.get("hwid") or "").upper()
    desc = (d.get("description") or "").lower()
    for needle, score, label in USB_SCORES:
        if needle in hwid:
            return score, label
    # Fall-back name matches for devices that don't surface VID:PID
    # cleanly (older Windows drivers, USB hubs).
    if "esp" in desc:                              return 50, "ESP* (by description)"
    if "silicon labs" in desc or "cp210" in desc:  return 50, "CP210x (by description)"
    if "ch340" in desc or "wch.cn" in desc:        return 50, "CH340 (by description)"
    if "ftdi" in desc:                             return 50, "FTDI (by description)"
    return 0, "unknown"


def find_candidates(pio: str) -> list[tuple[dict, int, str]]:
    devices = list_pio_devices(pio)
    scored = []
    for d in devices:
        score, label = score_device(d)
        if score > 0:
            scored.append((d, score, label))
    scored.sort(key=lambda t: -t[1])
    return scored


def hint_when_empty() -> None:
    info("No likely ESP32 found. Things to check:")
    info("  - is the USB-C cable a *data* cable (not a charge-only one)?")
    info("  - if your DevKit uses a CP210x or CH340 chip, did you install")
    info("    the matching USB-UART driver?")
    info("      CP210x: https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers")
    info("      CH340:  https://www.wch.cn/downloads/CH341SER_EXE.html")
    info("  - some devkits need a button dance to enter the ROM bootloader:")
    info("    hold BOOT, tap RESET, release BOOT, then re-run this script.")
    info("  - re-run with `--port <name>` if you know the port already")
    info("    (COM5 on Windows, /dev/ttyUSB0 or /dev/ttyACM0 on Linux,")
    info("     /dev/cu.usbserial-* on macOS).")


def pick_port(args: argparse.Namespace, pio: str) -> str:
    if args.port:
        info(f"Using --port {args.port}")
        return args.port

    candidates = find_candidates(pio)
    if not candidates:
        hint_when_empty()
        fail("No matching USB device.")

    if len(candidates) == 1:
        d, score, label = candidates[0]
        info(f"Found 1 candidate:")
        info(f"  {d['port']}  —  {label}  (score {score})")
        if args.yes:
            return d["port"]
        try:
            ans = input("    Flash this port? [Y/n] ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            fail("Aborted.")
        if ans and ans not in ("y", "yes", "o", "oui"):
            fail("Aborted.")
        return d["port"]

    info(f"Found {len(candidates)} candidates:")
    for i, (d, score, label) in enumerate(candidates, start=1):
        info(f"  [{i}] {d['port']}  —  {label}  (score {score})")
    while True:
        try:
            raw = input("    Pick one [1]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            fail("Aborted.")
        if not raw:
            return candidates[0][0]["port"]
        if raw.isdigit() and 1 <= int(raw) <= len(candidates):
            return candidates[int(raw) - 1][0]["port"]
        info("Enter a number from the list, or press Enter for [1].")


# ---------- main flow ------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="USB-flash the Bamboard firmware for the first time.",
    )
    p.add_argument(
        "--port",
        help="Serial port to flash (e.g. COM5, /dev/ttyUSB0). Skips detection.",
    )
    p.add_argument(
        "-y", "--yes",
        action="store_true",
        help="Don't ask for confirmation when a single port is auto-picked.",
    )
    p.add_argument(
        "--erase",
        action="store_true",
        help="Full chip erase before flashing — wipes Wi-Fi creds, "
             "Bambuddy URL and API key. Use when reusing a board.",
    )
    p.add_argument(
        "--monitor",
        action="store_true",
        help="Open the serial monitor after a successful flash so you can "
             "watch the captive-portal AP come up.",
    )
    p.add_argument(
        "--build-only",
        action="store_true",
        help="Compile without uploading — useful to validate the source "
             "before plugging the device in.",
    )
    return p.parse_args()


def run_pio(pio: str, extra_args: list[str]) -> int:
    cmd = [pio] + extra_args
    print()
    print("    $ " + " ".join(quote_arg(a) for a in cmd))
    print()
    return subprocess.run(cmd, cwd=str(FIRMWARE)).returncode


def main() -> int:
    require_python()
    args = parse_args()
    banner("Bamboard — first-install (USB flash)")

    stage("Locating PlatformIO")
    pio = find_pio()
    info(f"Using {pio}")

    if args.build_only:
        stage("Building firmware (no upload)")
        return run_pio(pio, ["run"])

    stage("Detecting USB device")
    port = pick_port(args, pio)

    if args.erase:
        stage("Erasing flash")
        info("This wipes every saved setting (Wi-Fi, Bambuddy URL, API key).")
        rc = run_pio(pio, ["run", "-t", "erase", "--upload-port", port])
        if rc != 0:
            fail(f"Erase failed (pio exit code {rc}).")

    stage("Building + flashing")
    info("PlatformIO compiles (if needed), then runs esptool over the chosen port.")
    started = time.monotonic()
    rc = run_pio(pio, ["run", "-t", "upload", "--upload-port", port])
    elapsed = time.monotonic() - started

    print()
    if rc != 0:
        stage(f"PlatformIO exited with code {rc} after {elapsed:.0f}s.")
        info("Common causes:")
        info("  - Wrong port: re-run without --port to re-detect.")
        info("  - Bootloader entry: hold BOOT, tap RESET, release BOOT, retry.")
        info("  - USB cable: try a different one (some are charge-only).")
        return rc

    stage(f"Flashed in {elapsed:.0f}s.")
    info("Next steps:")
    info("  1. The device boots into setup mode and exposes a Wi-Fi AP.")
    info("  2. Connect any phone/laptop to `Bamboard-setup` (password `bamboard`).")
    info("  3. The captive portal opens automatically — fill in your home")
    info("     Wi-Fi credentials, plus your Bambuddy URL and API key.")
    info("  4. Done. The device reboots into the live dashboard.")
    info("After that, use scripts/update-* for subsequent flashes over the LAN.")

    if args.monitor:
        print()
        stage("Opening serial monitor (Ctrl-C to leave)")
        return run_pio(pio, ["device", "monitor"])

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()
        print("Aborted.")
        sys.exit(130)
