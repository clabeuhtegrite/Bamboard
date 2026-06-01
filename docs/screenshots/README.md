# Screenshots

Real 480 × 272 captures of every Bamboard screen, rendered by the host simulator
(`sim/`) against **live Bambuddy data** and dumped to PNG — the same CI harness
that validates the UI on every change. These are the actual widgets the firmware
builds at runtime, not mockups.

| File                | Screen                                                                  | Tab idx |
|---------------------|-------------------------------------------------------------------------|---------|
| `live.png`          | Live dashboard — temps, progress, ETA, camera thumbnail; Pause/Stop + light while printing | 0 |
| `ams.png`           | AMS overview — outlined slots, clear-filament checkerboard, prev/next chevrons, Dry pill | 1 |
| `printers.png`      | Multi-printer list, focused row highlighted                             | 2 |
| `queue.png`         | Print queue — Bambuddy's pending jobs, in order                         | 3 |
| `history.png`       | Stats KPIs + recent archives                                            | 4 |
| `settings.png`      | Diagnostics (incl. Bambuddy server version) + brightness 1–5 + factory reset | 5 |
| `camera.png`        | Full-screen printer-camera viewer (tap the Live progress ring / thumbnail) | — |
| `device_render.svg` | Hero render of the assembled device — slim PETG case, integrated desk-stand tab | — |

The pre-v0.16 hand-drawn `*_mock.svg` mockups have been replaced by these real
renders.

## Panel layout

- Canvas: 480 × 272 — the native JC4827W543 panel.
- Header: y = 0 .. 36 (1 px hairline at y = 35).
- Body: y = 36 .. 228 (192 px tall).
- Tab bar: y = 228 .. 272 (44 px), **six** tabs, 3 px accent strip above the active one.
- Colours / radii come from `firmware/src/config.h` (`C_*`, `R_PANEL` / `R_PILL` / `R_CHIP`).

## Regenerating

Trigger the **sim** workflow (or build `sim/` locally with the `BAMBUDDY_URL` /
`BAMBUDDY_API_KEY` / `CF_ACCESS_*` env vars set) and copy the PNGs from the
`sim-screens` artifact over the ones here.
