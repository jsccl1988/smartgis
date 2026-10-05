<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-ui-opt — reference

## Matrix model

```
同等壳层物料  (dark ThemeService; SmartGIS.exe --ui-showcase=…)
        │
        ├─ L1b views_bench          role = perf (ns/iter)
        │     hover / table / overlay_crop / compositor_smoke
        │
        ├─ shell / catalog / data   role = perf (PaintCounters ms)
        │     UI_SHOWCASE_LINGER_MS=0
        │
        ├─ scene                    role = smoke  ← 3D tab visual
        │
        └─ interact                 opt-in only; visual review

Leftover/legacy hosts are frozen. Do not launch SmartGIS-Legacy.exe.
Map2d / Scene3D GPU peers belong to other harness-auto-* skills.
```

## Fairness

| Claim | OK? |
| --- | --- |
| Rank shell vs catalog on `commit_ms`/`raster_ms`/`present_ms` | Yes |
| Rank `BM_hover_commit` vs `BM_table_scroll_commit` on `real_time_ns` | Yes |
| Rank chrome vs `hud_fps` or map2d `present_gpu_warm_ms` | **No** |
| Rank by process `wall_ms` | **No** |
| Treat `scene` / `interact` as chrome PaintCounters peers | **No** |
| Launch leftover SmartGIS-Legacy for UI chrome | **No** |

## Perf JSON (showcase)

Leaf: `captures/ui/ui-showcase-<mode>-perf.json`

Written at end of `run_ui_present_capture` from `ui::gfx::paint_counters()` (Debug QPC accumulate → ms).

| Field | Meaning |
| --- | --- |
| `commit_ms` / `raster_ms` / `present_ms` | Shell compositor stages |
| `widget_paint_ms` | Widget `on_paint` |
| `map_paint_ms` | Map viewport paint (FALSE-GAP vs chrome) |
| `hud_fps` | Identity HUD cadence (map, not chrome) |
| `layout_count` / `create_font` / `create_brush` | Allocation / layout pressure |
| `overlay_copy_bytes` | Shell BGRA crop (U3) |

## Artifact layout

```
out/Debug/captures/analysis/ui_opt/matrix/
  views_bench.json
  shell/ catalog/ data/ scene/
  ui_profile_matrix.csv
  ui_profile_matrix.json
  MATRIX.md
  RECOMMEND.md
```
