<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views controls: public toolkit vs app composition

**Date:** 2026-09-13  
**Status:** accepted (three-phase implementation)  
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
| `src/render/skia` | Canvas backend for Views chrome (not a widget kit) | `render::skia` |

Includes stay `"ui/views/foo.h"`. No public nest `src/ui/views/controls/` or `src/ui/views/widget/`. Physical folders `kernel/` / `primitives/` / `gis/` under `src/ui/views` hold `.cc` only; public headers stay at the module root (see `src/ui/views/README.md`).

```
src/app/views          compose Widget + toolkit widgets
        │
        ▼
src/ui/views           public ui::views (flat includes)
  *.h at root; kernel/ | primitives/ | gis/ hold .cc only
  View / Widget / Theme / primitives
  CatalogView / LayerTree / AttributeTable / FeatureInfo / StatusBar
  AmboxView / ChartView
  MapViewport (native HWND hang; attach modes unchanged)
        │
        ▼
src/render/skia        fill / text only
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
| Theme | Optional dark `Theme::current()`; GIS widgets include `"ui/views/theme.h"` |
| Native combo | No HWND combobox; Combobox is a View + child list |
| File/message | Win32 `GetOpenFileNameW` / `MessageBoxW` |
| Tests | `views_unittests` (no MFC); app `--self-test` unchanged |

---

**最后更新：** 2026-09-14
