# CLAUDE.md

Guidance for Claude Code (and humans) working in this repository.

## What Bamboard is

A budget (~20 €) open-source touch desk monitor for **Bambu Lab** 3D printers,
built on a **Guition JC4827W543** all-in-one board (ESP32-S3-WROOM-1 + 4.3"
480×272 IPS panel (NV3041A, QSPI) + GT911 capacitive touch). It talks **only** to a
self-hosted **Bambuddy** instance (REST API) — never a third-party
cloud. UI is LVGL 8.3; firmware self-updates over the air from this repo's
GitHub Releases.

## Repo layout

```
firmware/        PlatformIO project (ESP32-S3 + LVGL + Arduino_GFX) — the device
  src/hw/        Display + GT911 touch HAL (Arduino_GFX + TouchLib)
  src/net/       Bambuddy REST client, GitHub OTA, host validation, CA bundle
  src/ui/        LVGL screen manager, per-screen builders, i18n (en/es/fr/pt/de), fonts
  src/config.h   ALL compile-time tunables (pins, palette, poll cadence, OTA, reboot)
  src/main.cpp   Boot, Wi-Fi provisioning (captive portal), FreeRTOS tasks, boot-time OTA
sim/             Host LVGL simulator — recompiles the REAL UI against shims, renders PNGs
tests/           Host unit tests (CMake/ctest) for pure logic (no HW/network)
web/             Browser-based flasher (ESP Web Tools) → GitHub Pages
scripts/         validate_i18n.py, compose_hero.sh
.github/         CI: ci.yml (build), tests.yml (unit+cppcheck+i18n+visual), sim.yml, pages.yml, release.yml
case/ hardware/ docs/   Enclosure (CC BY-NC-SA), BOM, guides
```

## Build / test / lint commands

The cloud `SessionStart` hook (`.claude/hooks/session-start.sh`) installs the
toolchain and pre-builds the sim + tests, so these work out of the box in a
remote session.

```bash
# Firmware build (ESP32-S3). Generate the embedded CA bundle first (git-ignored).
cd firmware && bash scripts/gen_ca_bundle.sh && pio run -e esp32-s3-devkitc-1

# Host unit tests
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release && cmake --build tests/build -j
ctest --test-dir tests/build --output-on-failure

# Host simulator — renders each screen to a PNG (Claude's visual-validation harness)
cmake -S sim -B sim/build -DCMAKE_BUILD_TYPE=Release && cmake --build sim/build -j
SIM_FIXTURES_ONLY=1 SIM_OUT=fx_out ./sim/build/bamboard_sim   # deterministic, no network

# Static analysis (exactly as CI runs it)
cppcheck --enable=warning --std=c++17 --inline-suppr --error-exitcode=1 \
  --suppress=missingInclude --suppress=missingIncludeSystem --suppress=unknownMacro \
  --suppress=unusedStructMember --suppress=unmatchedSuppression \
  -i firmware/src/ui/fonts firmware/src

# Web + i18n validation
node --check web/i18n.js && node --check web/app.js
python3 scripts/validate_i18n.py
```

## Visual regression (important)

`tests.yml` byte-compares the sim's deterministic renders against committed
baselines in `tests/fixtures_baseline/` (and per-language under
`tests/fixtures_baseline/i18n/<lang>/`). **Any intentional UI change must
re-seed the affected baselines**, or CI fails. Re-render with the sim
(`SIM_FIXTURES_ONLY=1`, and `BAMBOARD_LANG=<lang>` for the i18n set) and commit
the new PNGs in the same change. This is why many feature commits are paired
with a `test(visual): reseed …` commit.

## Conventions

- **`config.h` is the source of truth** for tunables — colours, radii, poll
  cadence, timings. Reuse the `ui::C_*` / `R_*` / `H_*` tokens; don't hardcode.
- **Threading:** UI task (core 1, watchdog-guarded, must never block) vs. net
  task (core 0, owns the REST client + blocking HTTP). Touch handlers
  enqueue control actions onto a FreeRTOS queue (`ui::ctrl::enqueue`); the net
  task drains them (`control_process`). Never do network I/O on the UI task.
- **Secrets** (Bambuddy URL/key, Wi-Fi, CF Access token) live in NVS via the
  captive portal — never in source or `config.h`.
- **i18n:** all 5 languages must stay key-aligned (`validate_i18n.py` enforces
  it); only Latin-script fits the bundled `bb_font_*`.
- **Versioning:** CI injects the version from the git `v*` tag; a local build is
  the `0.0.0-dev` sentinel (disables boot-time OTA). Update `CHANGELOG.md` for
  user-visible changes (Keep a Changelog format).
- The sim/tests reuse the firmware sources directly, so keep `ui/` and `net/`
  host-compilable (the `sim/shim/` shims stand in for Arduino/ESP APIs).

## Git

Develop on the assigned feature branch; commit with clear messages; push with
`git push -u origin <branch>`. Do **not** open a PR unless explicitly asked.
