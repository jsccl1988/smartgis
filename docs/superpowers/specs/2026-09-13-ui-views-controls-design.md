<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views controls: public toolkit vs app composition

**Date:** 2026-09-13  
**Status:** accepted (three-phase implementation); **Updated:** 2026-09-28 — § UI compositor thread folded here (dated twin archived). **Nesting note superseded** by [`2026-09-19-ui-views-subdir-responsibility-design.md`](2026-09-19-ui-views-subdir-responsibility-design.md) (headers live under `kernel` / `primitives` / `dialogs` / `gis` / `map`; includes `"ui/views/<area>/foo.h"`)  
**Scope:** Make `src/ui/views` a reusable public toolkit (`ui::views`) and move GIS chrome panels out of hand-painted app code. This document covers all three phases at a high level. Product C++ is not specified here in full; follow the in-tree headers.

## Goal

`ui::views` is the endgame desktop widget kit (scheme 3 variant (b) in [`docs/build/ui-shell-multiprocess.md`](../../build/ui-shell-multiprocess.md)). Phase 1 makes the kernel and primitive controls real. Phase 2 adds public GIS widgets. Phase 3 is app composition only: `src/app/views` hosts a `Widget`, lays out toolkit widgets, and does not paint business panels by hand. Capability now lands in parallel (see the migration spec); the phase list is inventory, not a serial gate.

Every chrome scheme in the multiprocess note stays supported: leftover MFC, Views+Skia, WinUI, and MapViewport attach modes (`kContentMapView`, `kOopRender`, `kLocalDevice`, `kPlaceholder`). WebView2 / `src/web` were removed. Qt is banned. Chromium and Skia are not vendored.

## Layering

| Tree | Role | Namespace |
| --- | --- | --- |
| `src/ui/views` | Public toolkit: View/Widget/layout/events, primitives, GIS widgets, `MapViewport`, `Theme` | `ui::views` |
| `src/app/views` | Product chrome exe (`out/SmartGisViews.exe`) | `app` (composition only) |
| `src/app/winui` | Scheme 2 host | — |
| `src/app/` leftover | MFC `SmartGis.exe` | `Smt_*` |
| `src/ui/gfx` | Canvas backend for Views chrome (not a widget kit) | `ui::gfx` |

Includes use `"ui/views/<area>/foo.h"` (see 2026-09-19 subdir design). Do **not** add ad-hoc nests like `src/ui/views/controls/` or `src/ui/views/widget/`. Responsibility partitions `kernel/` / `primitives/` / `dialogs/` / `gis/` / `map/` / `testing/` hold colocated headers and sources (see `src/ui/views/README.md`).

```
src/app/views          compose Widget + toolkit widgets
        │
        ▼
src/ui/views           public ui::views (partitioned includes)
  views.h umbrella; kernel/ | primitives/ | dialogs/ | gis/ | map/
  View / Widget / Theme / primitives / dialogs
  CatalogView / LayerTree / AttributeTable / FeatureInfo / StatusBar
  AmboxView / ChartView
  map/MapViewport (native HWND hang; attach modes unchanged)
        │
        ▼
src/ui/gfx        fill / text only
src/content + gpu      map session / OOP present (unchanged)
```

## Phase 1 — toolkit kernel + primitives

Kernel on `View` / `Widget`:

- Focus: `set_focusable` / `request_focus` / `Widget::focused_view` / Tab traversal / `on_focus` / `on_blur`
- Hover and pressed visual state
- `enabled` / `visible` (hidden views skip hit-test and paint)
- `invalidate` / `schedule_paint` → `InvalidateRect` on the Widget HWND
- `Theme` dark chrome (catalog blues/grays) and shared `utf8_to_wide` / `wide_to_utf8`
- Native-child hang preserved: `realize_native` / `realize_native_tree` / `create_native_view`

Primitives (usable, not paint-only stubs):

| Control | Behavior |
| --- | --- |
| `Label` | Text; optional color else Theme text |
| `Button` | Hover/press/disabled; click callback; Space/Return activate |
| `Textfield` | Focus, printable + backspace, caret, `set_change` |
| `Checkbox` | Toggle on click/Space; callback |
| `RadioButton` | Exclusive `group_id` among siblings; callback |
| `Combobox` | Child item list (not a native HWND combo); Up/Down cycles |
| `TabStrip` | Click tabs; layout/paint the active page |
| `TableView` | Columns/rows; selected row + click callback |
| `FilePicker` / `MessageBox` | Win32 dialogs (toolkit helpers) |

`//src/ui/views:views` stays a `source_set`, not in `src_all`. Console check: `views_unittests`.

## Phase 2 — public GIS widgets

These are public toolkit types (same include root). They consume `Theme` and primitives; they do not live in `src/app/views`.

| Widget | Role |
| --- | --- |
| `CatalogView` | Data-source / map-doc catalog chrome |
| `LayerTree` | Layer on/off and order |
| `AttributeTable` | Tabular attribute preview (`TableView`) |
| `FeatureInfo` | Selected-feature inspector |
| `StatusBar` | XY / scale / status text |
| `AmboxView` | Outlook toolbox (leftover `xambox`); groups of Label + Button |
| `ChartView` | Series chart (leftover `SmtChart` / `CDlg2DXChartView`) |

`MapViewport` remains the map hang. Phase 2 may document attach modes; it must not break `kChildHwnd` native-child or OOP/`content::MapView` paths.

`BUILD.gn` and `views.h` list these sources and headers in Phase 1 so the toolkit umbrella is complete. Phase 2 owns the file bodies.

## Phase 3 — app composition

`src/app/views` builds a `Widget`, attaches `MapViewport`, and **places** Phase 2 widgets. The app must not reimplement catalog / layer / attribute / feature-info / status / ambox / chart paint as local `View` subclasses.

`--self-test` on `SmartGisViews.exe` stays the e2e smoke path. Do not treat WinUI, WebView2, or leftover MFC as the destination toolkit.

## Multiprocess / chrome schemes (invariants)

- Scheme 1 WebView2 and scheme 2 WinUI keep their own hosts; they may later embed toolkit-equivalent presenters but are not rewritten here.
- Map pixels stay on `src/render` + `src/sdb` / `content::MapSession`. Views chrome does not `Init` a render device except through `MapViewport` attach.
- Default present remains shared-texture capable; `kChildHwnd` stays the in-process hang.
- One PE `--type=gpu|renderer` and leftover `SmartGisRender.exe` stay valid.

## Out of scope

- Pixel-perfect BCG / leftover MFC chrome; deleting leftover sources this cycle
- Chrome rewrite ownership and leftover retirement live in [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md) (`SmartGisViews.exe` is the destination entry; leftover `SmartGis.exe` stays until parity)
- Vendoring Chromium, Aura, Blink, or a Skia checkout
- Qt (Widgets / Quick / QML / any Qt module)
- Promoting WinUI, WebView2, or MFC Feature Pack to the endgame toolkit
- Putting `//src/ui/views:views` or the Views exe into `src_all`

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| Public API | `ui::views` (two levels; helpers in `detail` or anonymous) |
| Functions | `snake_case`; C++23 |
| Theme | Optional dark `Theme::current()`; GIS widgets include `"ui/views/kernel/theme.h"` |
| Native combo | No HWND combobox; Combobox is a View + child list |
| File/message | Win32 `GetOpenFileNameW` / `MessageBoxW` |
| Tests | `views_unittests` (no MFC); app `--self-test` unchanged |

---

## § UI compositor thread (Chromium roles)

**Status:** active  
**Updated:** 2026-09-28  
**Plan:** [`../plans/2026-09-28-ui-compositor-thread.md`](../plans/2026-09-28-ui-compositor-thread.md)  
**Folded from:** archived twin [`../archive/specs/2026-09-28-ui-compositor-thread-design.md`](../archive/specs/2026-09-28-ui-compositor-thread-design.md) (superseded — revise this § in place; do not open a new dated hot twin).

### § Shell present async (hover must not block)

`Widget::on_paint` **must not** call `ShellCompositor::wait_published`. Path:

1. UI: when pending dirty, `commit` + `notify_when_published(gen, hwnd)`.
2. UI: immediately `present` the published front (BitBlt); no front → empty frame until wake.
3. Worker: after publish, `PostMessage(kShellPublishedMessage)` (**wake only** — no BeginPaint / DestroyWindow).
4. UI: coalesce `InvalidateRect` on that message; next `WM_PAINT` BitBlts; `OnShellPublished(dirty)` only after `present` returns the new generation (same lock as BitBlt — do not re-read `published_generation()` afterward).

Close: `fire_will_close` clears host callbacks + awaiting state, then `shutdown()` (clear wake HWND + join worker), then `DestroyWindow`. Late `kShellPublishedMessage` is ignored via `will_close_fired_` / no compositor; do not `PostQuitMessage` from that path.

Worker must not own HWND; cross-thread **PostMessage wake** is allowed.

### Locked decisions

| # | Decision | End state |
| --- | --- | --- |
| 1 | Role map | **UI thread** (`Widget` HWND) · **Compositor thread** (pending / active + BeginFrame) · **Raster worker(s)** · **GPU / Display thread** (`draw_and_swap`, exclusive `rhi::Device`) |
| 2 | Frame path | UI Commit `DisplayList` snapshot → compositor pending → Activate → `CompositorFrame` (map quads + shell `kBgra` + HUD) → GPU Submit → present → BeginFrame |
| 3 | Reuse | `ui::gfx::DisplayList`; `Widget::shell_raster` / `ShellRaster`; `gpu::detail::CompositorFrame` / `DrawQuad`; `gpu::draw_and_swap`; `MapViewport` `frame_request_` / `gpu_present_` (today sync on UI; replace with Submit) |
| 4 | Phases | **P0→P5** gate → same-thread Commit → compositor thread → raster worker → GPU thread → DWM BeginFrame |
| 5 | Close | Stop BeginFrame → stop Commit → drain raster → drop unsubmitted → GPU drain / destroy device → `DestroyWindow`; hook `Widget::will_close` |
| 6 | Git | Work on `master`; no feature branch |

### Chromium role map

| Chromium role | SmartGIS | Duty (this phase) |
| --- | --- | --- |
| UI / main | UI thread: `Widget` owns HWND | Input, layout, `DisplayList` record, Commit snapshot; **no** HWND on workers |
| cc / compositor | Compositor thread | pending / active; BeginFrame; assemble `CompositorFrame` |
| Raster | Raster worker(s), start with **1** | dirty-rect replay → resources (`ShellRaster` / CPU) |
| GPU / viz display | GPU / Display thread | Exclusive `rhi::Device`; `draw_and_swap`; present |
| BeginFrame | DWM-aligned (**P5**, after P4) | After present; drive next Commit / Activate |

### Frame path

1. **UI Commit** — immutable `DisplayList` snapshot into compositor pending.
2. **Pending → Activate** — compositor thread (from P2); drop superseded unactivated frames.
3. **CompositorFrame** — map quads + shell `kBgra` + HUD.
4. **GPU Submit** — P4: `GpuPresentFn` / sync `gpu_present_` → Submit; GPU thread calls `draw_and_swap`.
5. **BeginFrame** — P5: DWM-aligned feedback.

### Type anchors

| Symbol | As-built | Role |
| --- | --- | --- |
| `ui::gfx::DisplayList` | `src/ui/gfx/display_list/` | Commit snapshot source |
| `ShellRaster` / `Widget::shell_raster` | `src/ui/gfx/raster/` + `Widget` | Shell pixels → `kBgra` |
| `PaintCommit` / `commit_view_tree` | `src/ui/views/kernel/paint/` | Immutable Commit |
| `ShellCompositor` | `src/ui/views/kernel/compositor/` | pending/active + 1 raster worker; present BitBlt |
| `Widget` | `src/ui/views/kernel/widget/` | HWND; Commit → present (async wake, no wait) |
| `gpu::detail::CompositorFrame` | `src/gpu/compositor/` | Activate IR |
| `gpu::draw_and_swap` | `src/gpu/frame_sink.h` | GPU-thread-only |
| `Widget::will_close` / `set_will_close` | `kernel/widget/` | Ordered shutdown |
| `PaintCounters` | `ui/gfx/raster/paint_stats.*` | P0 timing gate |

Kernel partitions (`view/` / `widget/` / `layout/` / `shell/` / `paint/` / `compositor/`): see [`2026-09-19-ui-views-subdir-responsibility-design.md`](2026-09-19-ui-views-subdir-responsibility-design.md) § Kernel Chromium-aligned partitions.

### Phases P0–P5

| Phase | Deliverable | Depends |
| --- | --- | --- |
| **P0** | commit / raster / present timings readable | — |
| **P1** | Immutable Commit; `WM_PAINT` replays only | P0 |
| **P2** | pending / active on compositor thread | P1 |
| **P3** | 1 raster worker; dirty-rect; no HWND (PostMessage wake OK) | P2 |
| **P4** | GPU thread owns device + `draw_and_swap`; Submit | P3 |
| **P5** | DWM BeginFrame | P4 |

### Close order

1. Stop BeginFrame (skip until P5). 2. Stop UI→compositor Commit. 3. Drain raster. 4. Drop unsubmitted frames. 5. GPU drain + destroy device. 6. `DestroyWindow` on UI thread. Failures must be observable; UI must not sync-block forever on GPU destroy.

### Non-goals

No Blink / cc property trees / Mojo viz; no Skia Ganesh for shell; no leftover `SmtGdiRenderThread`; no HWND ownership on worker / compositor / GPU (PostMessage wake only); no `--type=gpu` this phase; no Qt; agents do not run `build.bat` / `gn` / `ninja`.

### Acceptance (overview)

P0 timings visible; P1 paint = replay; P2 pending/active cross-thread; P3 ≥1 raster worker; P4 device + `draw_and_swap` on GPU thread; P5 BeginFrame after P4; human verify `build.bat` / `build.bat te`.

---

**最后更新：** 2026-09-28
