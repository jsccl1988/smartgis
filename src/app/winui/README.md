<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Scheme 2 chrome (`//src/app/winui`)

Unpackaged Win32 + Windows App SDK bootstrap. Product exe: `out/SmartGisWinui.exe`.

```bat
build.bat winui
```

Requires `smt_build_winui=true` (set by that alias). Not in `group("all")`.

## WinAppSDK

Pinned NuGet: **Microsoft.WindowsAppSDK 1.7.260224002** extracted at:

`third_party/windows_app_sdk/Microsoft.WindowsAppSDK.1.7.260224002/include/MddBootstrap.h`

Do not use the 2.x meta-package (no headers). C++/WinRT projections are generated into `out/winui_winrt` by `gen_winrt.bat` (Windows SDK `cppwinrt.exe`).

## IDE layout (aligned with Views)

Single-window product chrome (Fluent toolkit, same regions as Views):

```
MenuBar (File: Open/Exit · View: Map/Data/3D · Tools: Select/Draw/Clear/Pan)
  Catalog (TreeView + Refresh / Add layer) | Map Edit|Data|3D tabs + MapHost | Ambox
  Inspector tabs: FeatureInfo | AttributeTable
  StatusBar
```

Command ids match Workspace builtins (`selection.point`, `edit.append.point`,
`view.pan`, `view3d.trackball`, …) via `MapContents::ActivateTool`.

## Map host

- **Owned WS_POPUP overlay** (owner = top-level Win32 window), positioned in
  **screen** coords from `TransformToVisual` + `MapWindowPoints(island→desktop)`.
  A `WS_CHILD` sibling is covered by `DesktopChildSiteBridge` composition — the
  child paints the map but the user only sees the XAML slot background.
  Do not parent under the island (`DestroyWindow` / WER races).
- Present: `content::PresentMode::kSoftwareDib` — GPU publishes shared pixels;
  chrome blits `MapWidgetHostView::Latest()`, then overlays `app::MapScene`
  vectors (China city **polygon / line / point + name labels**, same as Views).
- 3D tab: same `Scene3dController` as Views — GPU publishes land-masked DEM
  (not flat MapLibre); chrome HUD / GDI fallback; FlyCube `present_gpu` when
  preferred — SmartGis.exe / Views parity.
- Startup: `activate()` pumps until `StartRenderProcess` Hello, then
  **synchronously** `attach_map()` (avoids DispatcherQueue race → white slot).
- Startup loads `china_city.gpkg` (fallback `china_city.geojson` / `china_plp`)
  beside the PE via `MapScene::seed_default`. File → Open also calls
  `MapHost::open_map_path` (OGR).
- Tabs: **Map Edit** (`kMapEdit`) / **Data** (`kMapData`) / **3D** (`kScene3d`).
  Views stay open across tab switches (no CloseView on each click); HWND island
  is re-synced via `sync_layout` only.
- `--self-test` requires IDE chrome + OOP GPU + live DIB pixels for **2D and 3D**
  (`map-frame-ok` / `scene-frame-ok` in `out/self-test-mark.txt`).
- OOP: relaunch this PE with `--type=gpu` via `MapContents::StartRenderProcess`.

Host ABI: `#include` `src/content/public` when present; otherwise local shim in `detail/`.

```bat
build.bat winui
out\SmartGisWinui.exe
out\SmartGisWinui.exe --self-test
```

Run `--self-test` with cwd = `out/` (GPU child + runtime DLLs resolve next to the PE).
