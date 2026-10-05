<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Desktop UI endgame: Chromium Views + Skia

User choice (2026-09-13): high-ceiling desktop chrome is **in-process C++**, Chromium-style **Views** (widget / layout / events) plus **Skia** (paint), hosting the existing map viewport (`src/render` + `src/sdb`).

**Product brand:** **SmartGIS Horizon** — next-generation / modern desktop GIS (Views + Skia destination shell). Engineering path stays `src/app/views/shell/`; do **not** introduce `src/chrome/`. Living lock: [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) §Horizon product brand.

This is the durable destination. **This pass ports leftover MFC chrome** into `ui::views`. `SmartGIS-Legacy.exe` stays leftover until parity; then stop compiling MFC UI. Do **not** wrap `CView`.

## Decision

| Role | Choice | Path | Namespace |
| --- | --- | --- | --- |
| Shell toolkit (endgame) | Chromium-style Views | `src/ui/views/` | `ui::views` |
| Shell paint | Skia canvas (backend only) | `src/ui/gfx/` | `ui::gfx` |
| Product chrome exe | `src/app/` hosts | `src/app/views/` → `out/SmartGIS.exe` | `app` |
| Map viewport | Hosted HWND (mgis `content::MapView` hang) | child HWND → `gis` + `render/{gdi,gl}` or OOP `SmartGisRender.exe` | legacy `Smt_*` / `content::` when present |
| Leftover MFC exe | `SmartGIS-Legacy.exe` until parity | `src/legacy/app/` | — |
| Legacy chrome | MFC Feature Pack / `src/legacy/ui` (retire after parity) | `src/legacy/ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | `Smt_*` |

**Nesting cap** stays `src/<layer>/<module>`. `src/app/views` is OK; do **not** add ad-hoc nests at the module root (e.g. `src/ui/views/widget/` outside `kernel/`). Under `src/ui/views`, responsibility partitions (`kernel` / `primitives` / `dialogs` / `map` / `markup` / `testing`) are public include roots; product GIS chrome lives in sibling module `src/ui/gis/` (`catalog` / `inspect` / `shell` / `style` / `analysis` / `debug` / `dialogs`). Chromium-aligned subgroups under them are allowed. They are not a third semantic UI namespace. Paint stays `src/ui/gfx` (`ui::gfx`) with the same style of public responsibility dirs (`geometry/` · `color/` · `canvas/` · `display_list/` · `raster/` · `image/` · `font/` · `animation/`); includes are `"ui/gfx/<area>/...."`. Skia is the optional canvas backend (`canvas/canvas_skia.cc`, `has_skia`), not a widget kit and not a third semantic namespace. See [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) and [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) § 职责子目录.

Local **mgis** (`c:\Dev\src\gis\mgis`) is WTL + `gui/` + `content::MapView` (`CreateParams { HWND parent_hwnd }`). **mogu** Chromium Views is not on this machine. Naming: `ui/views` = toolkit, `src/app/` = product shells, `ui/gfx` = shell canvas.

[`ui-shell-multiprocess.md`](ui-shell-multiprocess.md) scheme 3 variant **(b)** is this implementation. WinUI, CEF (`SmartGisCef.exe`), and C# WinUI (`SmartGisCs.exe`, `src/app/cs`) may exist as sibling product shells; they are **not** the Views toolkit destination. WebView2 chrome and `src/web` were removed. Qt is banned. CEF design: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md). C# host: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md).

## Rejected / alternate (do not “helpfully” switch)

| Path | Status |
| --- | --- |
| **Qt** (Widgets / Quick / QML) | Banned. `.cursor/rules/repo/no-qt.mdc`. |
| **MFC Feature Pack** (`CMFC*`) | Bridge only for leftover `SmartGIS-Legacy.exe`. **Not** the destination toolkit. |
| **WinUI 3 + WinAppSDK** | Sibling prototype only; not the endgame. |
| **C# WinUI 3 host** (`SmartGisCs.exe`) | Sibling embedder; not the endgame. |
| **WebView2 shell + native map** | Sibling prototype only; not the endgame. |
| Skia **as a widget kit** | No. Skia paints; Views owns widgets. |
| Vendoring Chromium / Skia wholesale | Out of scope. |
| Split a separate browser-shell tree vs leftover `src/app/` | Rejected. Hosts live in `src/app/{views,winui,cef,cs}`. |


`build.bat app` / `build.bat views` build the destination chrome (`SmartGIS.exe`). Leftover MFC：`build.bat legacy_app`（`build_app`）。

## Layering (paths + responsibilities)

```
src/app/views/                product chrome (SmartGIS.exe only)
  compose Widget + Splitter + tabs + public GIS widgets + MapViewport
  (does not paint catalog / ambox / chart / layer panels by hand)

src/ui/gis/                      product GIS chrome (same ui_views.dll / UI_EXPORT)
  catalog/ inspect/ shell/ style/ analysis/ debug/
  catalog/ also hosts Create*/AddBasemap modals; inspect/ hosts AttributeSchema
  include: "ui/gis/<area>/...." — namespace still ui::views for now

src/ui/views/                    toolkit (opt-in //:ui_views → ui_views.dll)
  views.h / views.cc             — umbrella only at root
  kernel/ | primitives/ | dialogs/ | map/
                                 — headers + sources colocated (public partitions)
  markup/{style,document,layout,factory,loader}/
  testing/                       — harness, views_*tests, testdata goldens
  Widget, View, Splitter, layout, events, Theme (kernel/)
  primitives (Button, Label, Textfield, …)
  dialogs (Dialog, FilePicker, MessageBox, InputText, SelectOne only)
  map/DrawHost                — View that hosts the map HWND + ViewHost
  include: "ui/views/<area>/...." — //src on the include path

src/ui/resources/                product .ui.xml / .ui.css by area (GN → shared out/ui/<area>/)

src/app/{views,winui}/          endgame / prototype hosts only

src/legacy/app/                  leftover MFC SmartGIS-Legacy.exe + app_core

src/legacy/ui/{gui,mfc_ex,xview, LEGACY chrome + map CView (until parity)
        xcatalog,xambox,chart}

src/ui/gfx/                 shell paint (GDI-backed canvas in v1)
  geometry/ color/ canvas/ display_list/ raster/
  image/ font/ animation/   (thin stubs; not Chromium vendor)
  include: "ui/gfx/<area>/...."

src/content/public/              optional later: content::MapView
src/legacy/render/{gdi,gl,…}     leftover map/3D devices (optional DLL)
src/sdb/{map,feature,layer}      EXISTING map / layers / doc
```

GN: `//src/ui/views:views` + `//src/ui/gis:gis` + `//src/ui/gfx:gfx` via `//:ui_views`. `//src/app/views:views` (`out/SmartGIS.exe`) loads only when `build_views=true`. **Not** in `//src:src_all`, **not** a dep of GN `group("all")`.

## How the draw host hangs

**Today (legacy MFC):**

```
CMainFrame
  └── CSmartMapEditView : CView
        HWND
        RenderDevice2d::Init(hWnd)     src/render/{gdi,gl}
        Map                          src/sdb/map
```

**Scheme 3 (this chrome):**

```
src/app/views  (SmartGIS.exe — only product entry)
  ui::views::Widget                 native HWND (Win32)
    Splitter  (resizable; not true dock, not MDI)
      ├── AmboxView / CatalogView   public toolkit widgets
      └── TabStrip  Map edit 2D / Datasource / 3D
            DrawHost (View)      do not wrap CView
              child HWND
                content::ViewHost   command / input dispatch
                1) content::MapView when src/content/public exists
                2) CreateProcess SmartGisRender.exe (IMapSession ABI)
                3) LoadLibrary + RenderDevice2d::Init
                4) labeled placeholder
```

Same hang as mgis `content::MapView::CreateParams { HWND parent_hwnd }`: the shell gives the map a parent window; **GIS + render stay C++**. The map is not rewritten as Skia widgets and is not a wrapped `CView`. Shell paint is the `ui::gfx` stub (GDI fill/text), not a vendored Skia tree. Migration ownership: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md).

## Status (v1)

- **Canvas：** 壳 paint 默认 **GDI**（`canvas_gdi.cc`）；公开 API 经 `canvas.cc` 派发。`has_skia=true` + 本机 pin 时同链真 Skia（`canvas_skia.cc`），运行时 `--shell-canvas=gdi|skia` 或 `SHELL_CANVAS`（CLI 优先；默认 gdi；未链入则回落）。Views `paint_self` 无 `#ifdef`。几何在 `ui::gfx::geometry`。见 [`src/ui/gfx/README.md`](../../src/ui/gfx/README.md) 与 living [`2026-09-14-render-skia-canvas-design.md`](specs/2026-09-13-render-rhi-scene-design.md) § 运行时后端切换。
- Toolkit kernel: `Widget`, `View` tree, focus / hover / press / enabled / visible, `schedule_paint`, `Theme`, `FillLayout` / `BoxLayout`, mouse/key/char dispatch, Skia stub canvas.
- DPI: Per-Monitor V2 when available (`enable_process_dpi_awareness`), `WM_DPICHANGED` / `WM_GETDPISCALEDSIZE` on `Widget`, DIP→px helpers, preferred-size recompute on scale change, map host surface uses real window DPI (not hardcoded 96).
- BeginFrame: `ui::gfx::VblankClock` (`IDXGIOutput::WaitForVBlank`) paces `DrawHost` Display thread and `gpu::PresentMailbox`; Sleep(16) fallback. Shell Commit still follows `WM_PAINT` (not yet BeginFrame-driven).
- Primitives: `Label`, `Button`, `Textfield`, `Checkbox`, `RadioButton`, `Combobox`, `TabStrip`, `TableView`, plus Win32 `FilePicker` / `MessageBox`.
- GIS widgets (public, `src/ui/gis/`): `CatalogView`, `LayerTree`, `AttributeTable`, `FeatureInfo`, `StatusBar`, `AmboxView`, `ChartView` — see [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) §ui/gis layering move.
- Exe: `build.bat views` → `out/SmartGIS.exe` (destination entry). Console check: `views_unittests` and `SmartGIS.exe --self-test`.
- Default `build.bat` remains the 31 DLLs. All chrome schemes in [`ui-shell-multiprocess.md`](ui-shell-multiprocess.md) stay supported. Leftover `SmartGIS-Legacy.exe` compiles until parity.
- GUI 测试分层与门禁：[`ui-testing.md`](ui-testing.md)。

## Out of scope

- Vendoring Chromium, Aura, Blink, or a full Skia checkout.
- Qt, or treating WinUI / WebView2 / Feature Pack / C# WinUI host as the endgame.
- Pixel-perfect BCG / Feature Pack chrome.
- Deleting leftover MFC sources this pass; wrapping `CView`.
- Putting the chrome exe into `//src:src_all`.

---

**最后更新：** 2026-10-05
