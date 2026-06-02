# Visual-regression baselines

Golden renders of the simulator's deterministic fixtures (`SIM_FIXTURES_ONLY=1`
in `sim/main.cpp`). The `visual` job in `.github/workflows/tests.yml`
byte-compares fresh renders against these on every PR.

To intentionally update them (after a deliberate UI change): download the
`fixture-renders` artifact from the `tests` run and copy the PNGs over these.

| File | State |
|------|-------|
| `live.png`      | Live while printing — temps, progress, controls, camera thumbnail, fan readout |
| `ams.png`       | 2-unit AMS, unit 1 — outlined black + clear-PETG checkerboard swatches, drying countdown |
| `printers.png`  | Printers farm grid — progress rings, per-tile temps + ETA, state colours |
| `queue.png`     | Print queue — pending jobs in order with their target printer |
| `history.png`   | History & stats — last prints + lifetime KPIs |
| `settings.png`  | Settings — diagnostics, brightness, Wi-Fi-setup + factory-reset pills |
| `ambient.png`   | Ambient idle clock overlay (time, date, farm summary) |
| `tempgraph.png` | Live nozzle / bed / chamber temperature graph |
