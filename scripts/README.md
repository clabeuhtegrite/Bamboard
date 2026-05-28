# scripts/

One-click launchers for the two things Bamboard owners do regularly:

| Action | Windows | macOS | Linux |
|---|---|---|---|
| First USB flash | `flash-windows.bat` | `flash-mac.command` | `flash-linux.sh` |
| OTA update     | `update-windows.bat` | `update-mac.command` | `update-linux.sh` |

The launchers are thin shells: each one finds a Python 3 interpreter
(PlatformIO's bundled `~/.platformio/penv/` first, then the system one)
and forwards all arguments to one of the two Python entry points.

## Files

```
scripts/
├── _common.py         Shared helpers: banner / stage / find_pio / arg quoting.
├── flash.py           First-install USB flash brain — port discovery, esptool dispatch.
├── ota.py             OTA update brain — mDNS discovery + caching, espota dispatch.
│
├── flash-windows.bat  Windows launcher → flash.py
├── flash-mac.command  macOS launcher   → flash.py
├── flash-linux.sh     Linux launcher   → flash.py
│
├── update-windows.bat Windows launcher → ota.py
├── update-mac.command macOS launcher   → ota.py
└── update-linux.sh    Linux launcher   → ota.py
```

`scripts/.bamboard-cache.json` is created at runtime to remember the
last good OTA host; it's gitignored and safe to delete.

## Common flags

Every launcher just forwards `argv` to its Python entry point, so any
flag the underlying script understands works through the launcher too.

Shared by both scripts:

- `--build-only` — compile only, skip the upload.

`ota.py`:

- `--host <addr>` — bypass mDNS / cache, target a specific host.
- `--password <pwd>` — bypass the env var and default.
- `--reset` — clear the cached host and re-discover.

`flash.py`:

- `--port <name>` — bypass auto-detection (e.g. `COM5`, `/dev/ttyUSB0`).
- `-y`, `--yes` — auto-confirm when a single port was detected.
- `--erase` — full chip erase before flashing (wipes saved settings).
- `--monitor` — open the serial monitor after a successful flash.

## Environment variables

- `BAMBOARD_HOST` — default host for `ota.py` (skips discovery).
- `BAMBOARD_OTA_PASSWORD` — default OTA password (overrides the
  built-in `bamboard`).

## Adding a new launcher

Stick to the two existing shapes:

- **POSIX (`.sh` / `.command`)**: prefer `~/.platformio/penv/bin/python3`,
  fall back to `python3` on PATH, then exit with the standard install
  hint. Add `text eol=lf` in `.gitattributes` for the file.
- **Windows (`.bat`)**: prefer `%USERPROFILE%\.platformio\python3\python.exe`,
  fall back to `py -3`, then `python`. End with `pause` so a
  double-clicked window stays open long enough to read.
