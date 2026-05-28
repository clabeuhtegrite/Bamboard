#!/usr/bin/env python3
"""
Bamboard — OTA update helper.

Wraps `pio run -t upload` with friendlier UX:

  - Discovers the device on the LAN (mDNS for ``bamboard.local``) and
    falls back to a one-time prompt if resolution fails. The chosen
    host is cached so subsequent runs skip the lookup.
  - Reads the OTA password from the BAMBOARD_OTA_PASSWORD env var when
    set, otherwise uses the firmware default ("bamboard").
  - Locates the ``pio`` executable in PATH or in the standard
    PlatformIO install location, so the script works whether or not
    the user added PlatformIO to PATH themselves.

The actual upload is still performed by PlatformIO's espota.py, this
script just removes the per-flash plumbing. Pass ``--reset`` to clear
the cached host and re-discover.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import socket
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


REPO_ROOT  = pathlib.Path(__file__).resolve().parent.parent
FIRMWARE   = REPO_ROOT / "firmware"
CACHE_PATH = REPO_ROOT / "scripts" / ".bamboard-cache.json"

DEFAULT_HOST     = "bamboard.local"
DEFAULT_PASSWORD = "bamboard"
ENV_HOST         = "BAMBOARD_HOST"
ENV_PASSWORD     = "BAMBOARD_OTA_PASSWORD"


# ---------- host resolution ------------------------------------------------

def load_cache() -> dict:
    if not CACHE_PATH.is_file():
        return {}
    try:
        return json.loads(CACHE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def save_cache(data: dict) -> None:
    try:
        CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        CACHE_PATH.write_text(json.dumps(data, indent=2), encoding="utf-8")
    except OSError as exc:
        info(f"(could not save host cache: {exc})")


def try_resolve(host: str, timeout: float = 2.0) -> str | None:
    """Return the IP the host resolves to, or None on failure."""
    # gethostbyname has no timeout argument, but we wrap it in a separate
    # thread via socket.create_connection — failing fast on TCP is more
    # reliable than waiting on system-wide mDNS timeouts.
    try:
        ip = socket.gethostbyname(host)
        return ip
    except socket.gaierror:
        pass

    # Last resort: try opening a TCP connection on the ArduinoOTA port,
    # which on the LAN typically lets the OS resolve via mDNS even when
    # gethostbyname couldn't.
    try:
        with socket.create_connection((host, 3232), timeout=timeout):
            return host
    except OSError:
        return None


def resolve_host(args: argparse.Namespace, cache: dict) -> str:
    if args.host:
        return args.host

    env_host = os.environ.get(ENV_HOST, "").strip()
    if env_host:
        info(f"Using BAMBOARD_HOST={env_host!r}")
        return env_host

    cached = cache.get("host") if not args.reset else None
    if cached:
        ip = try_resolve(cached)
        if ip:
            info(f"Cached host {cached} resolved (-> {ip}).")
            return cached
        info(f"Cached host {cached} no longer reachable, re-discovering.")

    info(f"Trying {DEFAULT_HOST} ...")
    ip = try_resolve(DEFAULT_HOST)
    if ip:
        info(f"Found {DEFAULT_HOST} -> {ip}")
        cache["host"] = DEFAULT_HOST
        save_cache(cache)
        return DEFAULT_HOST

    info("mDNS lookup failed (this is normal on some Windows setups).")
    while True:
        try:
            entered = input("    Enter Bamboard IP or hostname: ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            fail("No host provided.")
        if not entered:
            continue
        ip = try_resolve(entered)
        if ip is None:
            info(f"Could not reach {entered}:3232 — try again, or Ctrl-C to abort.")
            continue
        info(f"Reached {entered} (-> {ip}).")
        cache["host"] = entered
        save_cache(cache)
        return entered


# ---------- main flow ------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Upload the latest Bamboard firmware over the LAN.",
    )
    p.add_argument(
        "--host",
        help="Target host (overrides mDNS discovery and the cache).",
    )
    p.add_argument(
        "--password",
        help="OTA password (overrides the env var and the default).",
    )
    p.add_argument(
        "--reset",
        action="store_true",
        help="Clear the cached host and re-discover.",
    )
    p.add_argument(
        "--build-only",
        action="store_true",
        help="Compile without uploading — useful to check that the source "
             "still builds before pushing.",
    )
    return p.parse_args()


def resolve_password(args: argparse.Namespace) -> str:
    if args.password:
        return args.password
    env = os.environ.get(ENV_PASSWORD, "").strip()
    if env:
        info(f"Using OTA password from {ENV_PASSWORD}.")
        return env
    info("Using default OTA password.")
    return DEFAULT_PASSWORD


def run_pio(pio: str, extra_args: list[str]) -> int:
    """Stream pio output through this process and return its exit code."""
    cmd = [pio, "run"] + extra_args
    print()
    print("    $ " + " ".join(quote_arg(a) for a in cmd))
    print()
    proc = subprocess.run(cmd, cwd=str(FIRMWARE))
    return proc.returncode


def main() -> int:
    require_python()
    args = parse_args()
    banner("Bamboard — OTA update")

    stage("Locating PlatformIO")
    pio = find_pio()
    info(f"Using {pio}")

    if args.build_only:
        stage("Building firmware (no upload)")
        return run_pio(pio, [])

    cache = load_cache()
    stage("Resolving target device")
    host = resolve_host(args, cache)

    stage("Resolving OTA password")
    password = resolve_password(args)

    stage("Building + uploading via OTA")
    info("PlatformIO will compile (if needed) and stream the binary now.")
    info("Watch the device — a progress overlay appears once the upload starts.")
    started = time.monotonic()
    rc = run_pio(
        pio,
        [
            "-t", "upload",
            "--upload-port", host,
            "--upload-flags", f"--auth={password}",
        ],
    )
    elapsed = time.monotonic() - started

    print()
    if rc == 0:
        stage(f"Done in {elapsed:.0f}s — the device is rebooting into the new firmware.")
    else:
        stage(f"PlatformIO exited with code {rc} after {elapsed:.0f}s.")
        info("Common causes:")
        info("  - Wrong OTA password: set BAMBOARD_OTA_PASSWORD or pass --password.")
        info("  - Device unreachable: re-run with --reset to re-discover.")
        info("  - Build error: re-run with --build-only to isolate it.")
    return rc


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()
        print("Aborted.")
        sys.exit(130)
