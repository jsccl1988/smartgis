<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# UI shell performance upgrade — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Close the highest-ROI gaps between SmartGIS Views shell paint and Chromium-class responsiveness for three product scenarios (shell hover, table scroll, map+shell overlay) — without vendoring Blink/`cc`/viz.

**Architecture:** Continue the existing `PaintCommit` → `ShellCompositor` (1 worker) → BitBlt / `ShellRaster` path. Upgrade in **independent waves U0–U5**; each wave ships with a measurable gate (`PaintCounters` + `views_bench` + optional Trace `ui.views`). Map pixels stay on content/RHI; shell stays CPU DisplayList unless a later wave explicitly opts in.

**Tech Stack:** C++23, GN, `ui::views` / `ui::gfx`, existing `ShellCompositor` / `MapViewport` / FlyCube present, `views_bench`, DiagnosticTools Trace (`ui.views`).

**Living spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) **§Shell perf upgrade waves**  
**Predecessor:** [`2026-09-28-ui-compositor-thread.md`](2026-09-28-ui-compositor-thread.md) (P0–P5 landed; Deferred items absorbed here as U3–U5)

## Global Constraints

- Work on `master`; no feature branch; commit only when the user asks.
- Do **not** vendor Chromium Views / Blink / `cc` property trees / Mojo viz / `--type=gpu`.
- Do **not** enable Skia Ganesh for shell in these waves (CPU DisplayList + optional CPU Skia remain).
- Namespace two layers: `ui::views`, `ui::gfx`; new functions `snake_case`.
- `//src/ui/views` must **not** hard-dep `//src/gpu`; shell→map glue stays in `src/app/views`.
- English comments; copyright year 2026; colocate `.h`/`.cc`.
- Prefer `build.bat debug` / `views_bench` / `views_unittests` for gates; do not use `SmartGIS.sln`.

## Approach (locked)

| Option | Verdict |
| --- | --- |
| A — Vendor / link Chromium Views+cc | **Reject** (repo ban; wrong product shape) |
| B — Big-bang property-tree compositor | **Reject** (YAGNI; months of risk for GIS shell) |
| C — Scenario-ordered waves on current ShellCompositor | **Accept** |

Wave order = ROI from scenario map: measure → record/dirty → table text → overlay unify → optional multi-raster → optional BeginFrame shell.

```
U0 measure ──► U1 dirty/record ──► U2 table text
                      │
                      └──► U3 map overlay ──► U4 multi-raster (opt)
                                    │
                                    └──► U5 BeginFrame shell (opt)
```

U1 and U3 may parallel after U0. U2 depends on U0 (+ prefers U1). U4/U5 only if U1–U3 gates still miss budget.

---

## File map

| Area | Primary files |
| --- | --- |
| Counters / bench | `src/ui/gfx/raster/paint_stats.*`, `src/ui/views/testing/bench/views_bench.cc` |
| Commit / dirty | `src/ui/views/kernel/widget/widget.*`, `src/ui/views/kernel/paint/paint_commit.*`, `src/ui/views/kernel/view/view.*` |
| DisplayList | `src/ui/gfx/display_list/display_list.*`, `src/ui/gfx/canvas/*` |
| Table | `src/ui/views/primitives/collection/table_view.*` |
| Compositor | `src/ui/views/kernel/compositor/shell_compositor.*` |
| Overlay glue | `src/app/views/shell/ui/pages/map_pages.cc`, `src/app/views/shell/ui/browser_view.*`, `src/ui/views/map/map_viewport.*` |
| GPU frame (U3) | `src/gpu/` FlyCube `DrawRequest.shell` / present path (app or content glue only from views) |
| Trace | existing `BASE_TRACE_EVENT(..., "ui.views")` sites |

---

## U0 — Measurement gate (do first)

**Scenario:** all three. **Exit:** numbers exist before optimization claims.

- [x] Extend `PaintCounters` (or adjacent) with scenario tags or separate histograms: `hover_commit_qpc`, `table_scroll_qpc`, `overlay_copy_bytes` / `overlay_commit_qpc`
- [x] Add `views_bench` cases:
  - hover-like: small dirty Button invalidate → Commit → `wait_published`
  - table: `TableView` with N rows × C cols, scroll dirty strip → Commit → publish
  - overlay: construct `ShellRaster`-sized buffer + crop memcpy path (mirror `commit_widget_shell_to_maps` sizes)
- [x] Document how to read Trace filter `UI` / category `ui.views` for `record_commit` / `present` / compositor raster
- [x] Capture a baseline row in plan or `docs/build/ui-testing.md` (machine-local soft numbers OK; no fake Chromium SLA)

**Verify:**

```bat
build.bat debug views_bench views_unittests
out\Debug\views_bench.exe --benchmark_filter=BM_
out\Debug\views_unittests.exe --gtest_filter=*Paint*:*Compositor*
```

---

## U1 — Dirty / record tighten (shell hover)

**Problem:** hover expands dirty or re-records too much of the view tree into DisplayList.  
**Target:** small control dirty → record only intersecting views; stable sibling lists reused when possible.

- [x] Audit `commit_view_tree` + `View` paint cache: ensure non-intersecting siblings skip record (already partial via dirty rect — harden tests)
- [x] Optional: per-View cached `DisplayList` invalidated only when `paint_self` inputs change (`needs_paint` / content version); Commit appends clones for clean subtrees
- [x] Keep `Widget::on_paint` non-blocking (`notify_when_published`, no `wait_published` on product path)
- [x] Unit: mutate one Button; assert Commit cmd count / dirty area bounded vs full-client baseline
- [x] Bench: U0 hover case improves vs U0 baseline (directionally; soft gate)

**Non-goals this wave:** glyph atlas, multi-worker, BeginFrame.

**Verify:** `views_unittests` + `views_bench` hover filter + manual MenuBar/Button hover under Trace.

---

## U2 — Table / text paint (attribute table scroll)

**Problem:** visible cells each emit `kText`; scroll = dense CPU text every frame.  
**Target:** reduce text raster work for steady scrolling without changing TableView API.

- [x] Prefer (pick one, document choice in PR/checklist):
  - **A (recommended first):** row-strip cache — raster visible rows to a small CPU bitmap strip; scroll by blit + paint only newly exposed rows
  - **B:** shared glyph/run cache in `ui::gfx` for repeated cell strings (harder; do after A if needed)
- [x] Keep `visible_row_span` as the only rows touched; invalidate strip on data/selection/DPI/theme change
- [x] Unit: scroll one row; assert paint/text op count does not scale with total `rows_.size()`
- [x] Bench: U0 table case improves; large row count (e.g. 10k) with ~30 visible stays interactive in smoke

**Non-goals:** virtual DOM, Chromium InkDrop, GPU text atlases.

**Verify:** `views_unittests` Table* + `views_bench` table filter + AttributeTable interactive smoke if available.

---

## U3 — Map + shell overlay unify (absorbs compositor Deferred)

**Problem:** shell DIB → crop/copy → `commit_shell_overlay`; HUD may still GDI-after-present; dual pacing.  
**Target:** one submitted shell quad when generation stable; fewer full-frame copies; land predecessor Deferred items.

Predecessor Deferred → this wave:

| Deferred item | U3 action |
| --- | --- |
| HUD-as-quad in submitted frame | Consume `commit_shell_overlay` in FlyCube/`GpuPresentFn` path; stop redundant GDI HUD when shell quad present |
| views ↔ gpu PresentMailbox merge | Keep merge **out** of `ui/views`; finish glue in `src/app/views` + content/gpu only |
| Shell gen skip | Harden: unchanged `shell_generation` + equal crop → no memcpy |

- [x] Throttle / coalesce `OnShellPublished` → `commit_widget_shell_to_maps` (one overlay commit per published gen; drop superseded)
- [x] Crop only the dirty rect region when overlay surface size matches (or document why full-buffer still required)
- [x] Wire HUD-as-quad end-to-end; feature flag or role check so Scene3d/GDI fallback stays safe
- [x] Counters: `overlay_copy_bytes` drops on hover-only shell dirty when gen skip works
- [x] Human: pan map while hovering shell chrome — no double-hud flicker; Trace shows single present path

**Verify:**

```bat
build.bat debug SmartGisViews
REM interactive: china map + hover chrome; DiagnosticTools Trace UI + present
build.bat debug te
```

---

## U4 — Multi-worker / tiled raster (optional)

**Problem:** large dirty (resize, full theme, big table strip) saturates the single compositor worker.  
**Only start if** U1–U3 green but large-dirty benches still miss soft budget.

- [x] Split large dirty into horizontal strips; N=2 workers replay into shared back DIB with non-overlapping rows (mutex only at publish)
- [x] Or: tile DisplayList replay into strip bitmaps then blit to back DIB
- [x] Stress: full-client Commit at 1920×1080; publish latency vs U0
- [x] Shutdown: drain all workers before `DestroyWindow` (extend existing `will_close` order)

**Non-goals:** property trees, OOP viz, Skia Ganesh.

---

## U5 — BeginFrame-driven shell Commit (optional)

**Problem:** shell Commit still keyed off `WM_PAINT`; map Display already vblank-paced.  
**Only start if** overlay/map tearing or redundant Commits remain after U3.

- [x] Drive shell Activate / next Commit cadence from Display BeginFrame / `VblankClock` (UI still records on dirty; compositor may coalesce to frame ticks)
- [x] Do not block UI thread on vblank
- [x] Counters: `begin_frame_to_shell_present_qpc` (or reuse existing present latency fields)
- [x] Ensure shutdown still stops BeginFrame before compositor join

**Non-goals:** replace Win32 `InvalidateRect` entirely; Chrome viz frame sink API.

---

## Explicit non-goals (all waves)

- Vendoring Chromium / Blink / `cc` / Aura / ash
- Skia Ganesh for shell chrome
- Making map pixels paint through Views DisplayList
- Qt / WinUI as substitute toolkit
- Absolute FPS SLA vs Chrome browser

---

## Suggested ship order (product)

| Priority | Wave | Why |
| --- | --- | --- |
| P0 | U0 | Without numbers, later waves are guesswork |
| P1 | U1 + U3 | Hover feel + map overlay cost (most user-visible) |
| P2 | U2 | Attribute / catalog tables under load |
| P3 | U4 / U5 | Only if benches still demand it |

---

## Done bar

- [x] U0 baselines recorded
- [x] U1 and U3 checkboxes green (or explicitly deferred with reason)
- [x] U2 green if table scroll is a product priority this cycle
- [x] Living § + `src/ui/views/README.md` / `docs/build/ui-views-skia.md` as-built blurbs updated when waves land
- [x] Predecessor plan Deferred rows marked absorbed / done
- [x] `build.bat debug te` (or scoped views tests + views e2e) green

Human verify:

```bat
build.bat debug
build.bat debug te
out\Debug\views_bench.exe
```
