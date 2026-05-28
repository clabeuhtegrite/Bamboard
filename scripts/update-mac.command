#!/usr/bin/env bash
# Bamboard — OTA update (macOS).
#
# The .command extension makes this double-clickable from Finder; from a
# terminal you can also invoke it directly. Prefers PlatformIO's bundled
# Python over the system one so a fresh checkout works without extra
# install steps.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIO_PYTHON="$HOME/.platformio/penv/bin/python3"

if [ -x "$PIO_PYTHON" ]; then
    "$PIO_PYTHON" "$SCRIPT_DIR/ota.py" "$@"
elif command -v python3 >/dev/null 2>&1; then
    python3 "$SCRIPT_DIR/ota.py" "$@"
else
    cat <<'EOF'

!! Could not find a Python 3 interpreter.
   Install PlatformIO Core first:
     https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html

EOF
    exit 1
fi
