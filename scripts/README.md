# scripts/

One-click launcher for the first (and only) thing every Bamboard owner
does over USB: the initial flash. After that the device updates itself
over the air from GitHub Releases — see
[docs/flashing.md](../docs/flashing.md) — so there's no update launcher
here anymore.

| Action | Windows | macOS | Linux |
|---|---|---|---|
| First USB flash | `flash-windows.bat` | `flash-mac.command` | `flash-linux.sh` |

The launchers are thin shells: each one finds a Python 3 interpreter
(PlatformIO's bundled `~/.platformio/penv/` first, then the system one)
and forwards all arguments to the Python entry point.

## Files

```
scripts/
├── _common.py         Shared helpers: banner / stage / find_pio / arg quoting.
├── flash.py           USB flash brain — port discovery, esptool dispatch.
│
├── flash-windows.bat  Windows launcher → flash.py
├── flash-mac.command  macOS launcher   → flash.py
└── flash-linux.sh     Linux launcher   → flash.py
```

## Flags

Every launcher forwards `argv` to `flash.py`, so any flag it understands
works through the launcher too:

- `--port <name>` — bypass auto-detection (e.g. `COM5`, `/dev/ttyUSB0`).
- `-y`, `--yes` — auto-confirm when a single port was detected.
- `--erase` — full chip erase before flashing (wipes saved settings).
- `--monitor` — open the serial monitor after a successful flash.
- `--build-only` — compile only, skip the upload.

## Adding a new launcher

Stick to the two existing shapes:

- **POSIX (`.sh` / `.command`)**: prefer `~/.platformio/penv/bin/python3`,
  fall back to `python3` on PATH, then exit with the standard install
  hint. Add `text eol=lf` in `.gitattributes` for the file.
- **Windows (`.bat`)**: prefer `%USERPROFILE%\.platformio\python3\python.exe`,
  fall back to `py -3`, then `python`. End with `pause` so a
  double-clicked window stays open long enough to read.
