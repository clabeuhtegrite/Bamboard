#!/bin/bash
# Bamboard — SessionStart hook for Claude Code on the web.
#
# Installs the toolchain the project's CI gates rely on so tests, linters and
# the host simulator work inside a fresh remote session, and pre-warms the
# heavy downloads (PlatformIO platform, sim FetchContent deps) so they land in
# the container's cached state instead of stalling the first in-session build.
#
# The C++ toolchain (cmake / g++), python3 and node are already present in the
# remote image, so this only adds what's missing. Idempotent + non-interactive.
set -euo pipefail

# Web-only: a local machine already has the developer's own toolchain, and we
# don't want to apt-install / pip-install behind the user's back there.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  echo "session-start: not a remote session — skipping toolchain setup"
  exit 0
fi

ROOT="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
cd "$ROOT"

echo "session-start: preparing Bamboard cloud toolchain in $ROOT"

# --- 1. System packages (static analysis + sim libcurl + hero compose) ------
# build-essential / cmake are already in the image; listed for completeness so
# the hook still works on a leaner base. cppcheck = the firmware linter (CI),
# libcurl dev = the sim's optional real-Bambuddy fetch, imagemagick = hero compose.
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update -qq
sudo apt-get install -y -qq \
  build-essential cmake \
  cppcheck \
  libcurl4-openssl-dev \
  imagemagick

# --- 2. Python tooling (firmware build) -------------------------------------
# PlatformIO drives the ESP32-S3 firmware build; esptool merges the factory
# image. (The root-CA bundle's `cryptography` dep is installed by
# gen_ca_bundle.sh itself in step 3 — kept off this line so its --upgrade can't
# trip over a distro-managed cryptography that has no pip RECORD to uninstall.)
pip install --quiet --upgrade platformio "esptool>=4.7,<5"

# Make sure pio is on PATH for the rest of this script and the session. pip
# drops console scripts in a base/user bin that may not be on PATH yet.
PIO_BIN="$(python3 -c 'import sysconfig,os;print(os.path.join(sysconfig.get_path("scripts"),"pio"))' 2>/dev/null || true)"
if ! command -v pio >/dev/null 2>&1 && [ -n "$PIO_BIN" ] && [ -x "$PIO_BIN" ]; then
  export PATH="$(dirname "$PIO_BIN"):$PATH"
  [ -n "${CLAUDE_ENV_FILE:-}" ] && \
    echo "export PATH=\"$(dirname "$PIO_BIN"):\$PATH\"" >> "$CLAUDE_ENV_FILE"
fi

# --- 3. Root-CA bundle (required to build the https firmware) ---------------
# platformio.ini embeds firmware/data/cert/x509_crt_bundle.bin; it's git-ignored
# and regenerated per build. Generate it now so `pio run` links cleanly.
( cd firmware && bash scripts/gen_ca_bundle.sh )

# --- 4. Pre-warm the host simulator (Claude's visual-validation harness) ----
# Configuring fetches LVGL / ArduinoJson / stb via CMake FetchContent; building
# both validates the toolchain and caches the result. This is the main loop for
# UI work in a remote session (no hardware to flash), so keep it ready.
cmake -S sim -B sim/build -DCMAKE_BUILD_TYPE=Release
cmake --build sim/build -j"$(nproc)"

# --- 5. Pre-warm the host unit tests ---------------------------------------
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build -j"$(nproc)"

# --- 6. Pre-warm the PlatformIO firmware build -----------------------------
# Installs the espressif32 platform + pinned libraries (a large, one-time
# download) so the cached container has them and the first `pio run` doesn't
# stall mid-session. Non-fatal: a transient registry/network hiccup here must
# not block the (already-working) host gates above.
( cd firmware && pio pkg install -e esp32-s3-devkitc-1 ) \
  || echo "session-start: WARNING pio pkg install failed — run it manually before 'pio run'"

echo "session-start: toolchain ready"
