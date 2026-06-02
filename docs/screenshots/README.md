# Screenshots

The README's per-screen captures are **not committed here** — they're rendered
by the host simulator (`sim/`) from a built-in **demo dataset** (see
`render_fixtures()` in `sim/main.cpp`) and published to **GitHub Pages** on every
release, so they always match the current UI without bloating the repo with
binaries. The README links them at
`https://clabeuhtegrite.github.io/Bamboard/screenshots/<screen>.png`.

| Screen     | Demo state shown                                                            |
|------------|----------------------------------------------------------------------------|
| `live`     | Live dashboard mid-print — temps, progress, ETA, fans, light, Pause/Stop/speed |
| `ams`      | Multi-unit AMS; a drying 4-slot unit (outlined black + clear-PETG checker)  |
| `printers` | A 3-printer farm; printing rows show inline temps + ETA                     |
| `queue`    | Pending jobs with their target printer                                      |
| `history`  | Lifetime stats KPIs + recent prints                                         |
| `settings` | Diagnostics (incl. Bambuddy server version) + brightness + factory reset    |

Only the hero stays in this folder:

| File                | Notes                                                                       |
|---------------------|-----------------------------------------------------------------------------|
| `device_render.svg` | Hand-authored vector product shot of the assembled device (the README hero). Its Live camera tile embeds `sim/demo/camera.jpg` (base64) so the hero shows a real frame. |

The **demo camera frame** lives at `sim/demo/camera.jpg`. The sim decodes it (via
the same TJpg_Decoder shim the device uses) and `render_fixtures()` injects it, so
the **Live** screenshot shows the inline camera thumbnail; the hero embeds the same
file. Replacing it changes the `live` baseline — refresh it like any UI change.

## How it works

- `sim/main.cpp` `render_fixtures()` drives the real `apply_*_payload` handlers
  with deterministic demo JSON and dumps the six screens to PNG. `getLocalTime()`
  and `millis()` are pinned in the shim so renders are byte-reproducible.
- `.github/workflows/pages.yml` builds the sim and renders the demo screens into
  the Pages site (`screenshots/`) — on `release: published`, `web/**` edits, and
  manual dispatch.
- `.github/workflows/tests.yml` (the `visual` job) renders the same demo screens
  on every PR and byte-diffs them against the golden images in
  `tests/fixtures_baseline/`, catching unintended UI changes.

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
