<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views L2 pixel goldens

PNG baselines for `views_pixel_tests.exe` (shell chrome only; no map viewport pixels).

| File | Scene |
| --- | --- |
| `button_label_row.png` | Label + Button row |
| `tab_strip_two_tabs.png` | TabStrip with second tab selected |
| `status_bar_fields.png` | StatusBar with scale / CRS / XY / message |
| `ambox_default_strip.png` | Default AmboxView toolbox strip |

## Regenerate

From repo root (same cwd as `build.bat te`):

```bat
out\views_pixel_tests.exe --update-goldens
```

Optional: set `SMARTGIS_ROOT` to the repo root if the test is launched from another cwd.

Compare uses WIC-loaded PNG vs offscreen GDI capture with per-channel tolerance (default ±2, ≤0.5% bad pixels).

---

**最后更新：** 2026-09-14
