<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/ui` — leftover MFC shell

One DLL (`dll_stem=ui_legacy`). Physically under `legacy/` so `src/ui/` only holds the endgame toolkit (`views/` + `gis/`).

B1 layout (2026-09-29): capability dirs mirror `src/ui/gis` / `views/map` vocabulary; scheme C; no shim; stay under `legacy/ui`. Sole AFX attach: `widgets/dll/dll_main.cpp`. `grid/` and `dock/` removed (Feature Pack).

## Capability map (endgame vocabulary, still under `legacy/`)

| Capability | ≈ endgame | Role here |
| --- | --- | --- |
| `shell/` | `app/views/shell` + frame PCH | `xview` / `input_dispatch` + `shell.rc` |
| `shell/ambox/` | `ui/gis/shell` AmboxView | `title` + `tree` + `outlook_bar` |
| `shell/chart/` | `ui/gis/shell` ChartView | leftover stat chart (`diagram_data` / `view_dlg`) |
| `map/` | `ui/views/map` | Map / 3D `CView` (`map_sources`; ex-`viewport/`) |
| `inspect/` | `ui/gis/inspect` | config / edit docks — deepen in §11h `{host,sys,edit}` |
| `catalog/` | `ui/gis/catalog` | trees + mgr + dialogs (`tree/` `ds/` `map/` `scene/`) |
| `dialogs/` | `ui/gis/{catalog,inspect}` modals + `ui/views/dialogs` | `toolkit/` · `gis/` · `detail/` + `dialogs_api` (§11i) |
| `widgets/` | (leftover-only FP glue) | `feature_pack/` + `prop/` + `dll/` (sole `DllMain`) |
| `res/<cap>/` | `ui/resources` | icons / `.rc2` / bitmaps (`res/shell/{ambox,chart}/`) |

| Item | Value |
| --- | --- |
| GN | `//src/legacy/ui:ui_legacy` |
| Include | `"legacy/ui/<capability>/…"` |
| Gate | `smt_build_app` / `build.bat ui_legacy` |
| Default `src_all` | **no** |

`tool_group_sources` still compile into this DLL (link-cycle avoid). Endgame Views must not depend on this tree. Feature Pack is a leftover bridge only.

Historical GN aliases: `map:viewport`, `inspect:panels`, `shell/ambox:xambox`, `shell/chart:stat_chart`.

### `shell/` deep layer (§11f)

```
shell/
  xview.*           SmtXView CView chrome base
  input_dispatch.*  Win32 → ViewHost
  ambox/
    title.*         CP936/UTF-8 captions
    tree.*          SmtXAMBox
    outlook_bar.*   SmtAMBoxMgrDocBar
  chart/
    chart.*  chart_api.*  diagram_data.*  view_dlg.*  chart.rc
```

`catalog/` (scheme C, colocated headers; PCH/`catalog.rc` at the capability root):

```
catalog/
  catalog_api.*     LayerMgrAppend / Remove
  tree/             SmtXCatalog
  ds/               datasource tree + create/select DS
  map/              map-doc tree + MapMgr + layer/map dialogs
  scene/            3D-object tree + SceneMgr
```

### `dialogs/` deep layer (§11i)

```
dialogs/
  dialogs_api.*           Smt*Dlg export facade
  toolkit/                ≈ ui/views/dialogs
    input_text_dialog.*
    select_one_dialog.*
  gis/                    ≈ ui/gis/catalog + inspect product modals
    feature_info_dialog.*
    att_struct_dialog.*   # leftover; endgame AttributeSchemaDialog
  detail/                 ogr_field_type · feature_info_grid
```

### `inspect/` deep layer (§11h)

```
inspect/
  host/prop_host.*        shared prop-list CWnd chrome
  sys/sys_config_dock.*   + flash_styles
  edit/edit_config_dock.*
```

`inspect/` (§11h deep layer; PCH at capability root; own `inspect_sources`):

```
inspect/
  host/     shared prop-list CWnd chrome (prop_host.h)
  sys/      SysConfigDockBar + flash_styles
  edit/     EditConfigDockBar
```

`widgets/` (§11d deep layer; PCH/RC at capability root):

```
widgets/
  feature_pack/   CBCGP* → CMFC* bridge (feature_pack.h)
  prop/           property-grid helpers (prop_list.h, prop_value.h)
  dll/            sole DllMain + AFX ext anchors
```
