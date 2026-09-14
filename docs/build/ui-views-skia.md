<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Desktop UI endgame: Chromium Views + Skia

User choice (2026-09-13): high-ceiling desktop chrome is **in-process C++**, Chromium-style **Views** (widget / layout / events) plus **Skia** (paint), hosting the existing map viewport (`src/render` + `src/sdb`).

This is the durable destination. **This pass ports leftover MFC chrome** into `ui::views`. `SmartGis.exe` stays leftover until parity; then stop compiling MFC UI. Do **not** wrap `CView`.

## Decision

| Role | Choice | Path | Namespace |
| --- | --- | --- | --- |
| Shell toolkit (endgame) | Chromium-style Views | `src/ui/views/` | `ui::views` |
| Chrome paint | Skia canvas (backend only) | `src/render/skia/` | `render::skia` |
| Product chrome exe | `src/app/` hosts | `src/app/views/` → `out/SmartGisViews.exe` | `app` |
| Map viewport | Hosted HWND (mgis `content::MapView` hang) | child HWND → `gis` + `render/{gdi,gl}` or OOP `SmartGisRender.exe` | legacy `Smt_*` / `content::` when present |
| Leftover MFC exe | `SmartGis.exe` until parity | `src/legacy_app/` | — |
| Legacy chrome | MFC Feature Pack / `src/legacy_ui` (retire after parity) | `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | `Smt_*` |

**Nesting cap** stays `src/<layer>/<module>`. `src/app/views` is OK; do **not** add `src/ui/views/widget/` or `src/app/views/widget/` as a public nest. Paint stays `src/render/skia` (not `src/ui/gfx`) so Skia remains a render backend, not a third UI nest.

Local **mgis** (`c:\Dev\src\gis\mgis`) is WTL + `gui/` + `content::MapView` (`CreateParams { HWND parent_hwnd }`). **mogu** Chromium Views is not on this machine. Naming: `ui/views` = toolkit, `src/app/` = product shells, `render/skia` = canvas.

[`ui-shell-multiprocess.md`](ui-shell-multiprocess.md) scheme 3 variant **(b)** is this implementation. WinUI may exist as a sibling prototype; it is **not** the destination. WebView2 chrome and `src/web` were removed. Qt is banned.

## Rejected / alternate (do not “helpfully” switch)

| Path | Status |
| --- | --- |
| **Qt** (Widgets / Quick / QML) | Banned. `.cursor/rules/repo/no-qt.mdc`. |
| **MFC Feature Pack** (`CMFC*`) | Bridge only for leftover `SmartGis.exe`. **Not** the destination toolkit. |
| **WinUI 3 + WinAppSDK** | Sibling prototype only; not the endgame. |
| **WebView2 shell + native map** | Sibling prototype only; not the endgame. |
| Skia **as a widget kit** | No. Skia paints; Views owns widgets. |
| Vendoring Chromium / Skia wholesale | Out of scope. |
| Split `src/chrome/` vs leftover `src/app/` | Rejected. Hosts live in `src/app/{views,winui}`. |

`build.bat app` / `build.bat views` build the destination chrome (`SmartGisViews.exe`). Leftover MFC：`build.bat legacy_app`（`smt_build_app`）。

## Layering (paths + responsibilities)

```
src/app/views/                product chrome (SmartGisViews.exe only)
  compose Widget + Splitter + tabs + public GIS widgets + MapViewport
  (does not paint catalog / ambox / chart / layer panels by hand)

src/ui/views/                    toolkit (opt-in //:ui_views)
  kernel/ | primitives/ | gis/   — physical folders only (not a public nest)
  root *.h                       — thin stubs; include stays "ui/views/foo.h"
  Widget, View, Splitter, layout, events, Theme
  primitives (Button, Label, Textfield, …)
  GIS widgets (CatalogView, LayerTree, AttributeTable, AmboxView, ChartView, …)
  MapViewport                    — View that hosts the map HWND + ViewHost
  include: "ui/views/...."       — //src on the include path

src/app/{views,winui}/          endgame / prototype hosts only

src/legacy_app/                  leftover MFC SmartGis.exe + app_core

src/legacy_ui/{gui,mfc_ex,xview, LEGACY chrome + map CView (until parity)
        xcatalog,xambox,chart}

src/render/skia/                 Skia backend (GDI-backed canvas in v1)
  canvas / paint for Views chrome
  include: "render/skia/...."

src/content/public/              optional later: content::MapView
src/render/{gdi,gl,render3d,…}   EXISTING map/3D devices
src/sdb/{map,feature,layer}      EXISTING map / layers / doc
```

GN: `//src/ui/views:views` and `//src/render/skia:skia` are always-loaded source_sets via `//:ui_views`. `//src/app/views:views` (`out/SmartGisViews.exe`) loads only when `smt_build_views=true`. **Not** in `//src:src_all`, **not** a dep of GN `group("all")`.

## How the map viewport hangs

**Today (legacy MFC):**

```
CMainFrame
  └── CSmartMapEditView : CView
        HWND
        SmtRenderDevice::Init(hWnd)     src/render/{gdi,gl}
        SmtMap                          src/sdb/map
```

**Scheme 3 (this chrome):**

```
src/app/views  (SmartGisViews.exe — only product entry)
  ui::views::Widget                 native HWND (Win32)
    Splitter  (resizable; not true dock, not MDI)
      ├── AmboxView / CatalogView   public toolkit widgets
      └── TabStrip  Map edit 2D / Datasource / 3D
            MapViewport (View)      do not wrap CView
              child HWND
                content::ViewHost   command / input dispatch
                1) content::MapView when src/content/public exists
                2) CreateProcess SmartGisRender.exe (IMapSession ABI)
                3) LoadLibrary + SmtRenderDevice::Init
                4) labeled placeholder
```

Same hang as mgis `content::MapView::CreateParams { HWND parent_hwnd }`: the shell gives the map a parent window; **GIS + render stay C++**. The map is not rewritten as Skia widgets and is not a wrapped `CView`. Chrome paint is the `render::skia` stub (GDI fill/text), not a vendored Skia tree. Migration ownership: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).

## Status (v1)

- **Canvas：** 壳 paint 默认 **GDI stub**（`canvas.cc` / `gdi32`）：已具备 `fill_rect` / `stroke_rect` / `draw_line` / `draw_text` / `measure_text` / `clip_rect` / `save` / `restore`；Views focus ring / ChartView 轴已消费描边与线 API。阶段 D：真后端 TU `canvas_skia.cc`；本机 pin 为 WSL graphic-engine/skia 的目录符号链接；匹配的 Windows `skia.lib` 已由本机 MSVC 最小 CPU 构建落到 `third_party/.src/skia_out`（须 `/MDd` 对齐产品 CRT）。`smt_has_skia=true` 时可链接并跑 `views_unittests`；**默认仍关**；不进 `src_all` / `render_all`。见 [`src/render/skia/README.md`](../../src/render/skia/README.md)。
- Toolkit kernel: `Widget`, `View` tree, focus / hover / press / enabled / visible, `schedule_paint`, `Theme`, `FillLayout` / `BoxLayout`, mouse/key/char dispatch, Skia stub canvas.
- DPI: Per-Monitor V2 when available (`enable_process_dpi_awareness`), `WM_DPICHANGED` / `WM_GETDPISCALEDSIZE` on `Widget`, DIP→px helpers, preferred-size recompute on scale change, map host surface uses real window DPI (not hardcoded 96).
- Primitives: `Label`, `Button`, `Textfield`, `Checkbox`, `RadioButton`, `Combobox`, `TabStrip`, `TableView`, plus Win32 `FilePicker` / `MessageBox`.
- GIS widgets (public): `CatalogView`, `LayerTree`, `AttributeTable`, `FeatureInfo`, `StatusBar`, `AmboxView`, `ChartView` — see [`docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`](../superpowers/specs/2026-09-13-ui-views-controls-design.md). Chrome port: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).
- Exe: `build.bat views` → `out/SmartGisViews.exe` (destination entry). Console check: `views_unittests` and `SmartGisViews.exe --self-test`.
- Default `build.bat` remains the 31 DLLs. All chrome schemes in [`ui-shell-multiprocess.md`](ui-shell-multiprocess.md) stay supported. Leftover `SmartGis.exe` compiles until parity.

## Out of scope

- Vendoring Chromium, Aura, Blink, or a full Skia checkout.
- Qt, or treating WinUI / WebView2 / Feature Pack as the endgame.
- Pixel-perfect BCG / Feature Pack chrome.
- Deleting leftover MFC sources this pass; wrapping `CView`.
- Putting the chrome exe into `//src:src_all`.

---

**最后更新：** 2026-09-14
