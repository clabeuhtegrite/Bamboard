# Screenshots

1:1 SVG mockups of every Bamboard screen, at the native JC4827W543 panel
resolution (**480 × 272**). They mirror the widgets the firmware builds at
runtime, populated with representative placeholder data so every screen
looks alive.

| File                 | Screen        | Tab idx |
|----------------------|---------------|---------|
| `dashboard_mock.svg` | Live dashboard (default screen, segmented speed chip visible) | 0 |
| `ams_mock.svg`       | AMS overview with prev/next chevrons + slot row | 1 |
| `printers_mock.svg`  | Multi-printer list, focused row highlighted | 2 |
| `history_mock.svg`   | Stats KPIs + recent archives | 3 |
| `settings_mock.svg`  | Diagnostics + brightness 1–5 + factory reset | 4 |
| `device_render.svg`  | Hero render of the assembled v0.4 device — slim PETG case, integrated desk-stand tab, screen lit on the Live dashboard | — |

## Conventions

- Canvas: `viewBox="0 0 480 272"` — matches the real panel.
- Header: y = 0 .. 36 (with a 1 px hairline at y = 35).
- Body: y = 36 .. 228 (192 px tall).
- Tab bar: y = 228 .. 272 (44 px), with a 3 px accent strip above the
  active tab at y = 228.
- Colours match `firmware/src/config.h` (`C_BG`, `C_PANEL`, `C_PANEL_HI`,
  `C_ACCENT`, `C_OK`, `C_WARN`, `C_ERR`, `C_TEXT`, `C_TEXT_DIM`).
- Radii from the v0.4 theme: panels = 12 px, pills = 18 px, chips = 8 px.

When you build the unit, drop real photos in here (phone camera at ~30°,
or LVGL snapshot dump if you instrumented the firmware) and replace the
README references in the project root.
