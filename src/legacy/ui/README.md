<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/ui` — leftover MFC shell

One DLL (`dll_stem=ui_legacy`). Physically under `legacy/` so `src/ui/` only holds the endgame toolkit (`views/` + `gis/`).

B1 layout (2026-09-29): capability dirs mirror `src/ui/gis` / `views/map` vocabulary; scheme C; no shim; stay under `legacy/ui`. Sole AFX attach: `widgets/widgets_core.cpp`. `grid/` and `dock/` removed (Feature Pack).

## Capability map (endgame vocabulary, still under `legacy/`)

| Capability | ≈ endgame | Role here |
| --- | --- | --- |
| `shell/` | `app/views/shell` + frame PCH | xview shell / `shell.rc` |
| `shell/ambox/` | `ui/gis/shell` AmboxView | aux-module Outlook bar |
| `shell/chart/` | `ui/gis/shell` ChartView | leftover stat chart |
| `map/` | `ui/views/map` | Map / 3D `CView` (ex-`viewport/`) |
| `inspect/` | `ui/gis/inspect` | config / edit docks (ex-`panels/`) |
| `catalog/` | `ui/gis/catalog` | trees + mgr + dialogs |
| `dialogs/` | `ui/gis/dialogs` + toolkit dialogs | generic MFC dialogs + `dialogs_api` |
| `widgets/` | (leftover-only FP glue) | `bcg_cmfc.h` + sole `DllMain` |
| `res/<cap>/` | `ui/resources` | icons / `.rc2` / bitmaps (`res/shell/{ambox,chart}/`) |

| Item | Value |
| --- | --- |
| GN | `//src/legacy/ui:ui_legacy` |
| Include | `"legacy/ui/<capability>/…"` |
| Gate | `smt_build_app` / `build.bat ui_legacy` |
| Default `src_all` | **no** |

`tool_group_sources` still compile into this DLL (link-cycle avoid). Endgame Views must not depend on this tree. Feature Pack is a leftover bridge only.

Historical GN aliases: `map:viewport`, `inspect:panels`, `shell/ambox:xambox`, `shell/chart:stat_chart`.
