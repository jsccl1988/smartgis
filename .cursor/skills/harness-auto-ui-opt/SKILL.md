---
name: harness-auto-ui-opt
description: >-
  Profiles Views shell chrome (PaintCounters commit/raster/present plus
  views_bench hover/table/overlay) across --ui-showcase modes, emits
  screenshots and a comparison table, then writes optimization
  recommendations. Use when the user invokes /harness-auto-ui-opt, or says
  ui profile, UI 性能, Views chrome profile, PaintCounters, views_bench,
  shell compositor 优化建议, or asks for UI screenshots + paint phase table.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto UI opt（chrome profile × 优化建议）

**同等壳层物料（dark Views chrome），`views_bench` × `--ui-showcase`，给出截图、PaintCounters 对比表与优化建议**

Closed loop: build → run matrix → inspect PNGs → performance table → **recommendations** → (optional) optimize against chrome budgets → re-run.

Sibling of `harness-auto-world3d-opt` (Scene3D GPU peers). This skill owns **`src/ui` Views/Skia chrome** (`ui::views` / `ui::gfx` / `ui::gis` panels) plus product `--ui-showcase`. Do **not** rank leftover MFC, map2d GPU, or Scene3D `ms_per_present` as UI chrome peers.

## Authorization

When this skill is invoked, attached (`@harness-auto-ui-opt` / `/harness-auto-ui-opt`), or followed, the agent **MUST** run the matrix (and rebuild if binaries are missing) and deliver **screenshots + comparison table + recommendations** — do not defer the harness to the user.

## Hard rules

1. **Equal profile (locked):** `UI_THEME=dark`, `UI_SHOWCASE_LINGER_MS=0`, product `SmartGIS.exe --ui-showcase=<mode>`. Do **not** strip Catalog/Ambox/markup to fake commit ms.
2. **FALSE-GAP:** chrome metrics = `commit_ms` / `raster_ms` / `present_ms` + L1b `real_time_ns`. `hud_fps` / `map_paint_ms` are **map cadence** — never rank them vs chrome. Process `wall_ms` is not the peer metric.
3. **Axes (perf):** `views_bench` (`BM_hover_commit` / `BM_table_scroll_commit` / `BM_overlay_crop_memcpy` / `BM_shell_compositor_smoke`) · showcase **`shell` / `catalog` / `data`**. **`scene` = visual/smoke** (3D tab chrome; not a PaintCounters peer vs hover). **`interact` not default** (gesture visual; use `harness-visual-review`).
4. Prefer `build.bat debug src/app/views:views` then `build.bat debug src/ui/views:views_bench`. Compile lock **OFF**. Stay on **`master`**. Do **not** launch leftover `SmartGIS-Legacy.exe`.
5. CBM first (`smartgis`). Prefer `*.inspect.png` for `Read`.
6. Default this skill **recommends**; patch + re-run only if the user asks to optimize.

## Entry commands

```bat
.\build.bat debug src/app/views:views
.\build.bat debug src/ui/views:views_bench
py -3 testing/tools/harness/ui/run_ui_profile_matrix.py
```

Optional: `py -3 testing/tools/harness/ui/run_ui_profile_matrix.py --row shell` · `--skip-bench`.

Artifacts: `out/Debug/captures/analysis/ui_opt/matrix/` (`MATRIX.md`, `RECOMMEND.md`, csv/json, `*.inspect.png`). Showcase perf JSON leaves: `out/Debug/captures/ui/ui-showcase-<mode>-perf.json`.

## Env (benchmark wiring)

| Env | Values | Effect |
| --- | --- | --- |
| `UI_THEME` | `dark` (locked) | Product chrome score / ThemeService |
| `UI_SHOWCASE_LINGER_MS` | `0` | No extra linger in timed capture |
| `UI_FORENSICS` | `1` | `out/ui_forensics/` on layout issues |
| `TRACE` | `1` | Optional chrome JSON sibling for `ui.views` spans |

## Workflow

```
UI profile progress:
- [ ] 1. Build SmartGIS + views_bench
- [ ] 2. Run run_ui_profile_matrix.py
- [ ] 3. Confirm shell/catalog *.inspect.png
- [ ] 4. Print performance tables (bench ns + chrome ms)
- [ ] 5. Read inspect PNGs + RECOMMEND.md
- [ ] 6. Emit numbered optimization recommendations (see below)
- [ ] 7. Optimize mode (if asked): one hypothesis → fix in src/ui → rebuild → re-run
```

### Performance table (fill from MATRIX.md)

#### L1b

| Row | real_time_ns | pass |
| --- | ---: | --- |
| bench:BM_hover_commit | | |
| bench:BM_table_scroll_commit | | |
| bench:BM_overlay_crop_memcpy | | |
| bench:BM_shell_compositor_smoke | | |

#### Product chrome

| Row | commit_ms | raster_ms | present_ms | pass | inspect |
| --- | ---: | ---: | ---: | --- | --- |
| shell | | | | | |
| catalog | | | | | |
| data | | | | | |

### Smoke (not chrome performance)

| Row | note |
| --- | --- |
| scene | 3D tab capture; map GPU out of scope |
| interact | opt-in `--row` only; visual review |

### Recommendation template

Lead with hottest **chrome** phase, then hottest **L1b** bench. Map to §Shell perf waves:

| Hot signal | Wave | Prefer code under |
| --- | --- | --- |
| `commit_ms` / `BM_hover_commit` | U1 | `kernel/paint/paint_commit.*`, `kernel/widget/widget.cc` `record_commit` |
| `BM_table_scroll_commit` / `table_scroll_ms` | U2 | `primitives/collection/table_view.cc` |
| `BM_overlay_crop_memcpy` / `overlay_copy_bytes` | U3 | `app/views` overlay crop; compositor gen skip |
| `raster_ms` / `BM_shell_compositor_smoke` | U4 | `kernel/compositor/shell_compositor.cc` |
| `present_ms` / `begin_frame_to_shell_present_ms` | U5 | `ShellCompositor::present` / vblank |
| `create_font` / `create_brush` | cache | `kernel/shell/theme.*`, painters |
| `layout_count` | layout | `markup/layout/yoga_layout_manager.cc` |
| `hud_fps` / `map_paint_ms` | **out of scope** | `harness-auto-map2d-frame-opt` / scene3d-frame-opt |
| cold `wall_ms` to first map | **out of scope** | `harness-auto-bootstrap` |

Do not invent Chrome FPS SLA. Soft Debug guidance only: if `raster_ms` ≫ `commit_ms` on shell, prefer dirty-rect / U4 before rewriting Theme.

## Related

- Living §: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Shell perf · §UI chrome equal-profile
- As-built: `docs/superpowers/ui-testing.md` L1b
- Runner: `testing/tools/harness/ui/run_ui_profile_matrix.py`
- Counters: `src/ui/gfx/raster/paint_stats.*`
- Compositor: `src/ui/views/kernel/compositor/shell_compositor.*`
- Detail: [reference.md](reference.md)
