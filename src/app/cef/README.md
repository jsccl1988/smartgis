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

## Escape hatch (enabled when `SmartGisRender.exe` is beside this PE)

CEF Chromium also consumes `--type=gpu*`. If map OOP relaunches
`SmartGisCef.exe --type=gpu`, `CefExecuteProcess` can swallow it. When
`SmartGisRender.exe` (or `SmartGisRenderD.exe`) sits next to this exe,
`MapContents::StartRenderProcess` prefers that PE for map GPU. Browser entry
also skips `CefExecuteProcess` when `--pipe=` is present (MapContents child).

Build the render PE with `build.bat render` (sets `smt_build_render=true`).
Product default remains: same-PE ContentMain after `CefExecuteProcess` when the
render PE is absent.

## Include discipline

Chrome sources must **not** include `gis_map.h`, `rd_renderdevice.h`, or
leftover `SmtRenderDevice`. Map path is `content/public` only.
