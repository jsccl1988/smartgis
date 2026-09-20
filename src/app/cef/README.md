<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS CEF chrome (`SmartGisCef.exe`)

Third product shell alongside Views / WinUI: **CEF Binary Distribution** paints
HTML chrome; map tabs are sibling Win32 HWNDs via `CefMapSlot` →
`content::MapContents` / `ViewHost`. Not the endgame toolkit (Views + Skia is).

## Build

```bat
build.bat cef
```

Sets `smt_build_cef=true`. Requires the pin under `third_party/cef/binary/`
(see `third_party/cef/README.md`). If the pin is missing, `build.bat cef`
prints a clear error and does **not** silently produce an exe.

Default `build.bat` / `ninja all` / `src_all` **do not** build CEF.

GN flags:

| Flag | Default | Meaning |
| --- | --- | --- |
| `smt_build_cef` | `false` | Opt-in compile of `//src/app/cef:cef` |
| `smt_has_cef` | auto | `true` when `third_party/cef/binary/include/cef_version.h` exists |

## Run

```bat
out\SmartGisCef.exe
out\SmartGisCef.exe --self-test
```

Web assets are copied to `out/cef_web/`. Entry order is mandatory:
`CefExecuteProcess` → `content::ContentMain` (same PE for `--type=gpu|renderer`).

## Product chrome

HTML IDE regions match Views: MenuBar (Open/Exit/Map/Data/3D/Select/Draw/Clear),
Catalog (tree + Refresh/Add layer), Map Edit|Data|3D tabs, Ambox builtins,
Inspector (FeatureInfo / AttributeTable), StatusBar. Bridge command ids align
with `tool::Workspace`.

Startup seeds `out/china_plp.geojson` via shared `app::MapScene` (OGR) and
forwards the same path through `MapContents::CatalogCall`. The sample is a
normal map pack: **区 / 线 / 点 / 注记** layers painted over the GPU frame
on **Map / Data** tabs only. Catalog lists those four layers. Map HWND
right-click shows the 2D view menu (Zoom In / Out / Pan / Full / Refresh).
Open / Add layer pick a file and call `MapScene::open_path`.

### 3D tab (true DEM relief)

`kScene3d` uses shared `Scene3dController` → `gis::DemRaster` height mesh
(FlyCube `present_gpu` or GDI facets). Pitch / orbit must reveal mountains /
plateau — not a flat province MapScene tilted by the camera. 2D vector
drape-as-texture on the DEM mesh is deferred (GpuScene is solid-shaded today).

Verify: open the **3D** tab, drag to pitch — Tibet/west should rise vs east
coast; HUD shows `pitch` / `dist`. Restart `SmartGisCef.exe` after rebuild.

Wheel / drag / pinch land on `ChromeBridge` as `PointerEvent` (or topic
`map.pointer` / `map.gesture`) and are forwarded to `ViewHost` plus
`MapContents::Dispatch` (host `kPointerEvent`). Native HWND also handles
`WM_MOUSEWHEEL` / `WM_GESTURE` GID_ZOOM. HTML `#map-slot` is a layout hole;
the sibling map HWND presents the GIS frame.

## Escape hatch (enabled when `SmartGisRender.exe` is beside this PE)

CEF Chromium also consumes `--type=gpu*`. If map OOP relaunches
`SmartGisCef.exe --type=gpu`, `CefExecuteProcess` can swallow it. When
`SmartGisRender.exe` (or `SmartGisRenderD.exe`) sits next to this exe,
`BrowserMain` calls `MapContents::SetGpuExeOverride` so
`StartRenderProcess` prefers that PE. Browser entry also skips
`CefExecuteProcess` when `--pipe=` is present (MapContents child).

Build the render PE with `build.bat render` (sets `smt_build_render=true`).
Product default remains: same-PE ContentMain after `CefExecuteProcess` when the
render PE is absent.

## Include discipline

Chrome sources must **not** include `gis_map.h`, `rd_renderdevice.h`, or
leftover `SmtRenderDevice`. Map path is `content/public` only.
