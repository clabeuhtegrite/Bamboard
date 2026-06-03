# Contributing to Bamboard

Thanks for helping out! Bamboard is a small, self-contained firmware project, so
the contribution loop is quick once you know where things live. This guide
covers the dev setup, how to build/test/lint, the visual-regression workflow
(the one thing that trips people up), and the project conventions.

For an architectural overview, see [`CLAUDE.md`](CLAUDE.md); for runtime config,
[`docs/configuration.md`](docs/configuration.md).

## Ground rules

- **MIT-licensed code** ([`LICENSE`](LICENSE)). The enclosure under [`case/`](case/)
  and the hero render are **CC BY-NC-SA 4.0** (© TomE1337) — keep that boundary.
- **No secrets in the repo.** Bambuddy URL/key, Wi-Fi creds and the Cloudflare
  Access token are provisioned at runtime into NVS via the captive portal —
  never hard-coded in `config.h` or anywhere else.
- **`config.h` is the source of truth** for tunables (colours, radii, poll
  cadence, timings). Reuse the `ui::C_*` / `R_*` / `H_*` tokens; don't hardcode.
- Update [`CHANGELOG.md`](CHANGELOG.md) (Keep a Changelog format) for any
  user-visible change.

## Local setup

You need a C++17 toolchain, CMake and Python 3. For the firmware build you also
need [PlatformIO](https://platformio.org/); for the static-analysis gate,
`cppcheck`; for the host simulator, libcurl dev headers.

```bash
# Debian/Ubuntu
sudo apt-get install -y build-essential cmake cppcheck libcurl4-openssl-dev
pip install platformio
```

> Working from **Claude Code on the web**? You don't need any of this — the
> repo's `SessionStart` hook installs the whole toolchain for you. See
> [Cloud / web sessions](#cloud--web-sessions) below.

## Build / test / lint

These are exactly what CI runs (`.github/workflows/`), so green locally ≈ green
on the PR.

```bash
# Firmware (ESP32-S3). Generate the embedded CA bundle first (it's git-ignored).
cd firmware && bash scripts/gen_ca_bundle.sh && pio run -e esp32-s3-devkitc-1

# Host unit tests (pure logic — version compare, host validation, i18n,
# the REST-client JSON parsers, the drying table)
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release && cmake --build tests/build -j
ctest --test-dir tests/build --output-on-failure

# Host simulator — renders each screen to a PNG
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

## Visual regression — read this before changing the UI

`tests.yml` byte-compares the simulator's deterministic renders against
committed baselines in [`tests/fixtures_baseline/`](tests/fixtures_baseline/)
(and per-language under `tests/fixtures_baseline/i18n/<lang>/`). **Any intended
UI change must re-seed the affected baselines in the same PR**, or CI fails.

Re-render and commit the new PNGs:

```bash
# English baselines
SIM_FIXTURES_ONLY=1 SIM_OUT=tests/fixtures_baseline ./sim/build/bamboard_sim

# Per-language baselines (es / fr / pt / de)
for lang in es fr pt de; do
  SIM_FIXTURES_ONLY=1 BAMBOARD_LANG=$lang \
    SIM_OUT=tests/fixtures_baseline/i18n/$lang ./sim/build/bamboard_sim
done
```

Eyeball the diffs before committing — the baselines are the spec. This is why
many feature commits are paired with a `test(visual): reseed …` commit.

## Internationalisation

All five languages (en/es/fr/pt/de) must stay key-aligned —
`scripts/validate_i18n.py` enforces it, and the host test checks that every
string's `printf` specifiers match across languages. Add or change strings in
[`firmware/src/ui/i18n.*`](firmware/src/ui/). Only Latin-script languages fit
the bundled `bb_font_*` fonts. Then re-seed the per-language visual baselines.

## Coding conventions

- **Match the surrounding code** — comment density, naming, idiom. The codebase
  leans on explanatory comments that say *why*, not *what*.
- **Threading:** the UI task (core 1, watchdog-guarded) must never block. Network
  I/O lives on the net task (core 0). Touch handlers enqueue control actions via
  `ui::ctrl::enqueue`; the net task drains them. Never do network I/O on the UI
  task.
- **Host-compilable:** the sim and tests reuse `firmware/src/ui/**` and
  `firmware/src/net/**` directly, with `sim/shim/` standing in for the Arduino /
  ESP APIs. Keep those sources host-compilable (no new hard Arduino dependency in
  pure-logic code).

## Commits & pull requests

- Conventional-ish commit subjects: `feat(queue): …`, `fix(net): …`,
  `test(visual): reseed …`, `docs: …`, `ci: …`.
- One logical change per PR; pair UI changes with their baseline re-seed.
- Make sure the relevant gates above pass locally. Open against `main`.

## Cloud / web sessions

This repo is set up to be worked on from **Claude Code on the web**. A
`SessionStart` hook ([`.claude/hooks/session-start.sh`](.claude/hooks/session-start.sh),
registered in `.claude/settings.json`) runs when a remote session starts and:

1. installs the system toolchain the base image lacks (`cppcheck`,
   `libcurl4-openssl-dev`, `imagemagick`);
2. installs PlatformIO + esptool;
3. generates the root-CA bundle;
4. pre-builds the simulator and the host tests, and pre-installs the PlatformIO
   platform — so the heavy downloads are cached and the first build doesn't stall.

It's **web-only** (guarded by `CLAUDE_CODE_REMOTE`) and idempotent, so it never
touches a local checkout. After it finishes you can run any of the
[build/test/lint](#build--test--lint) commands immediately. If you change the
toolchain a session needs, update that hook in the same PR.
