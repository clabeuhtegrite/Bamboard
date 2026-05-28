#!/usr/bin/env bash
# Bamboard — first-install USB flash (Linux).
#
# Run from a terminal: `scripts/flash-linux.sh`. Prefers PlatformIO's
# bundled Python over the distro one so the script works on a fresh
# checkout without juggling pip / pipx.
#
# Note: on many distros, accessing /dev/ttyUSB* / /dev/ttyACM* requires
# membership in the `dialout` (Debian/Ubuntu) or `uucp` (Arch) group:
#     sudo usermod -aG dialout "$USER" && newgrp dialout

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIO_PYTHON="$HOME/.platformio/penv/bin/python3"

if [ -x "$PIO_PYTHON" ]; then
    exec "$PIO_PYTHON" "$SCRIPT_DIR/flash.py" "$@"
elif command -v python3 >/dev/null 2>&1; then
    exec python3 "$SCRIPT_DIR/flash.py" "$@"
else
    cat <<'EOF'

!! Could not find a Python 3 interpreter.
   Install PlatformIO Core first:
     https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html

EOF
    exit 1
fi
