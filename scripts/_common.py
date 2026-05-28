"""
Shared helpers for the Bamboard launcher scripts.

`ota.py` and `flash.py` each carry their own argument parsing and
device-discovery logic but the surface stuff — banner, stage / info /
fail output, locating ``pio``, quoting arguments for display — is
identical. Centralised here so the two scripts can't drift out of sync.

Underscore prefix on the filename keeps this module from looking like a
public entry point next to ``ota.py`` / ``flash.py``.
"""

from __future__ import annotations

import pathlib
import shutil
import sys


# ---------- pretty output --------------------------------------------------

def banner(title: str) -> None:
    print()
    print("========================================")
    print(f"  {title}")
    print("========================================")
    print()


def stage(msg: str) -> None:
    print(f"==> {msg}")


def info(msg: str) -> None:
    print(f"    {msg}")


def fail(msg: str) -> "NoReturn":
    print()
    print(f"!! {msg}")
    sys.exit(1)


def quote_arg(arg: str) -> str:
    """Shell-quote ``arg`` only when it actually contains whitespace, so
    the printed command stays readable for the common case where it
    doesn't."""
    return f'"{arg}"' if any(c.isspace() for c in arg) else arg


# ---------- PlatformIO discovery -------------------------------------------

PIO_INSTALL_HINT = (
    "Install PlatformIO Core:\n"
    "      https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html"
)


def find_pio() -> str:
    """Return a usable ``pio`` command string, or exit with a helpful hint.

    Tries PATH first, then the standard PlatformIO virtualenv install
    location (Windows / macOS / Linux all colocate it the same way)."""
    on_path = shutil.which("pio")
    if on_path:
        return on_path

    home = pathlib.Path.home()
    candidates = [
        home / ".platformio" / "penv" / "Scripts" / "pio.exe",   # Windows
        home / ".platformio" / "penv" / "bin"     / "pio",       # macOS / Linux
    ]
    for c in candidates:
        if c.is_file():
            return str(c)

    fail(f"Could not find the `pio` command.\n    {PIO_INSTALL_HINT}")


# ---------- Python version guard -------------------------------------------

def require_python(min_major: int = 3, min_minor: int = 7) -> None:
    if sys.version_info < (min_major, min_minor):
        print(f"ERR: Python {min_major}.{min_minor}+ required.")
        sys.exit(1)
