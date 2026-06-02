# Screenshots

The README's per-screen captures are **not committed here** — they're rendered
by the host simulator (`sim/`) from a built-in **demo dataset** (see
`render_fixtures()` in `sim/main.cpp`) and published to **GitHub Pages** on every
release, so they always match the current UI without bloating the repo with
binaries. The README links them at
`https://clabeuhtegrite.github.io/Bamboard/screenshots/<screen>.png`.

| Screen      | Demo state shown                                                           |
|-------------|---------------------------------------------------------------------------|
| `live`      | Live dashboard mid-print — temps, progress, ETA, fans, light, Pause/Stop/speed |
| `ams`       | Multi-unit AMS; a drying 4-slot unit (outlined black + clear-PETG checker) |
| `printers`  | A 3-printer farm grid; printing tiles show a progress ring + temps + ETA   |
| `queue`     | Pending jobs with their target printer                                     |
| `history`   | Lifetime stats KPIs + recent prints                                        |
| `settings`  | Diagnostics (incl. Bambuddy server version) + brightness + factory reset   |
| `ambient`   | The idle ambient clock overlay                                            |
| `tempgraph` | The live temperature graph overlay                                        |

## Hero render

The README/site hero is **composited in CI**, not committed: `scripts/compose_hero.sh`
warps the freshly-rendered `live.png` onto **TomE1337's case photo** (`case.jpg`)
in perspective, and Pages serves the result at `screenshots/hero.png`. So the
hero always shows the current live UI on the real enclosure, refreshed on every
deploy.

| File       | Notes                                                                       |
|------------|-----------------------------------------------------------------------------|
| `case.jpg` | TomE1337's product photo of the enclosure — **CC BY-NC-SA 4.0** (see [`../../case/LICENSE`](../../case/LICENSE)). Base image for the composited hero. |

The composited hero is a **derivative of `case.jpg`** and is therefore likewise
CC BY-NC-SA 4.0, with attribution to TomE1337 — not MIT like the rest of the code.

## Demo camera frame

`sim/demo/camera.jpg` is a real printer-camera still. The sim decodes it (via the
same TJpg_Decoder shim the device uses) and `render_fixtures()` injects it, so the
**Live** screenshot — and therefore the hero — shows the inline camera thumbnail.
Replacing it changes the `live` baseline.

## How it works

- `sim/main.cpp` `render_fixtures()` drives the real `apply_*_payload` handlers
  with deterministic demo JSON and dumps each screen to PNG. `getLocalTime()` and
  `millis()` are pinned in the shim so renders are byte-reproducible.
- `.github/workflows/pages.yml` builds the sim, renders the demo screens into the
  Pages site (`screenshots/`), and composites the hero (`compose_hero.sh`) — on
  `release: published`, `web/**` / `pages.yml` edits, and manual dispatch.
- `.github/workflows/tests.yml` (the `visual` job) renders the same demo screens
  on every PR, byte-diffs them against the golden images in
  `tests/fixtures_baseline/`, and also composes the hero so the perspective warp
  is exercised too.

## Updating after an intentional UI change

The `visual` job will fail (the demo render no longer matches the baseline).
Download the `fixture-renders` artifact from the `tests` run and copy the PNGs
over `tests/fixtures_baseline/`. Pages then picks up the new look on its next
deploy.

## Panel layout

- Canvas: 480 × 272 — the native JC4827W543 panel.
- Header: y = 0 .. 36 (1 px hairline at y = 35).
- Body: y = 36 .. 228 (192 px tall).
- Tab bar: y = 228 .. 272 (44 px), **six** tabs, 3 px accent strip above the active one.
- Colours / radii come from `firmware/src/config.h`.
