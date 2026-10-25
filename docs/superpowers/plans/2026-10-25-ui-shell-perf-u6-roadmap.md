<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# UI shell perf U6+ roadmap (implementation checklist)

**Status:** active  
**Date:** 2026-10-25  
**Living spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Shell perf U6+ Chromium-aligned roadmap  
**Diagram:** [`../diagrams/ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html)  
**Harness:** [`../../../.cursor/skills/harness-auto-ui-opt/SKILL.md`](../../../.cursor/skills/harness-auto-ui-opt/SKILL.md)

## Baseline (before U6)

- [ ] `build.bat debug src/app/views:views`
- [ ] `build.bat debug src/ui/views:views_bench`
- [ ] `py -3 testing/tools/harness/ui/run_ui_profile_matrix.py`
- [ ] Record machine-local notes in `out/Debug/captures/analysis/ui_opt/matrix/` (no invented SLA)

## Lane A — U6 (shell latency)

- [x] `Widget::begin_paint_update` / `end_paint_update` defer `InvalidateRect` until unlock
- [x] `PaintCounters`: `paint_cache_hit`, `paint_cache_miss`, `layouts_since_paint`, `paint_lock_deferred`
- [x] `View::ensure_commands_recorded` notes cache hit/miss
- [x] Theme change: walk tree `invalidate_commands` (existing `on_theme_changed`)
- [x] DPI / `set_device_scale_factor`: same `invalidate_commands` boundary as Theme
- [x] Showcase perf JSON exports U6 counters
- [ ] Re-run UI profile matrix; compare `shell` + `BM_hover_commit` vs baseline

## Lane B — U7 (partial replay; no cc/viz)

- [x] `DisplayList` spatial index for `append_from_clipped` when op count ≥ threshold
- [ ] `BM_shell_compositor_smoke` + catalog/data `raster_ms` check
- [x] Document optional animation sub-surface seam (kernel-only; map `native_view` unchanged)
- [ ] Re-run UI profile matrix

## Lane C — U8 (GIS chrome)

- [x] `CatalogView::relayout_layers_page` — single `layout_contents` drain, no redundant subtree `layout`
- [x] `TableView` row hover — `schedule_paint_rect` on old/new row bands
- [x] Inspect/catalog paths: avoid full-widget invalidate when row-strip cache suffices
- [ ] Re-run matrix `catalog` + `data` rows under equal-profile (Catalog/Ambox intact)

## Done bar

- [x] Living § + this plan + diagram updated
- [ ] U6–U8 code landed with green `build.bat debug` targets above
- [ ] Matrix artifacts present; residual risks noted in commit/chat summary
