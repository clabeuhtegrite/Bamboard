# Visual-regression baselines

Golden renders of the simulator's deterministic fixtures (`SIM_FIXTURES_ONLY=1`
in `sim/main.cpp`). The `visual` job in `.github/workflows/tests.yml`
byte-compares fresh renders against these on every PR.

To intentionally update them (after a deliberate UI change): download the
`fixture-renders` artifact from the `tests` run and copy the PNGs over these.

| File | State |
|------|-------|
| `fx_live_printing.png` | Live while printing — controls, light, fan readout, wall-clock ETA |
| `fx_printers.png`      | Printers list — the enriched at-a-glance line (temps + ETA + %) |
| `fx_ams_multi.png`     | 2-unit AMS, unit 1 — outlined black + clear-PETG checker swatches |
| `fx_ams_dry.png`       | AMS-HT (unit 2) — live drying countdown + Stop |
| `fx_hms.png`           | HMS full-screen flash (driven by the real refresh() state machine) |
