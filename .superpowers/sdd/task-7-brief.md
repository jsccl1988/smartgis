<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 7: Map service Views (`smartgis.map_service`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

- `src/plugin/map_service/service_mgr_dialog.h` `.cc`
- `src/plugin/map_service/svr_cfg_base_page.h` `.cc`
- `src/plugin/map_service/svr_cfg_tile_page.h` `.cc`
- `src/plugin/map_service/map_client_dialog.h` `.cc`
- `src/plugin/map_service/map_service_commands.h` `.cc`
- Add `source_set("map_service_views")` to `src/plugin/map_service/BUILD.gn` (keep leftover DLL).

## Do NOT touch

`host_test.cc`, `src/plugin/BUILD.gn`, leftover `dlg_*`, other domains. Do not create a new `CDlg2DXView` type.

## APIs

```cpp
namespace plugin {
bool register_map_service(content::PluginHost* host);
}
```

- `ServiceMgrDialog`: `TableView` of service names + `TabStrip` (`SvrCfgBasePage`, `SvrCfgTilePage`) + buttons install/start/stop/uninstall/restart that `host->execute` commands `map_service.install` / `start` / `stop` / `uninstall` / `restart`.
- `SvrCfgBasePage` leftover fields: name, map doc, log, xmin/ymin/xmax/ymax, SRS combobox. Review Map constructs `plugin::MapPreviewView`.
- `SvrCfgTilePage`: min/max level, tile width/height, cache url. View Tiles uses `MapPreviewView`.
- `MapClientDialog`: client canvas is `MapPreviewView` (not leftover `CDlg2DXView`).
- Leftover `SmtMapService` only from `map_service_commands.cc`. If leftover headers pull MFC into the Views source_set, stub command handlers to return false.

`register_map_service`: `map_service.manage` → dialog `map_service.mgr`; `map_service.client` → `MapClientDialog`.

## GN

```gn
source_set("map_service_views") {
  sources = [
    "map_client_dialog.cc", "map_client_dialog.h",
    "map_service_commands.cc", "map_service_commands.h",
    "service_mgr_dialog.cc", "service_mgr_dialog.h",
    "svr_cfg_base_page.cc", "svr_cfg_base_page.h",
    "svr_cfg_tile_page.cc", "svr_cfg_tile_page.h",
  ]
  include_dirs += [ "//src" ]
  deps = [
    "//src/content:content",
    "//src/plugin:host",
    "//src/plugin/widgets:widgets",
    "//src/ui/views:views",
  ]
}
```

Copyright 2026. `plugin` namespace. `snake_case`. English comments. No Qt. No leftover headers in page `.h` if they pull MFC.

Compile: `ninja -C out src/plugin/map_service:map_service_views`. Fix until green.

Report: `.superpowers/sdd/task-7-report.md`. No commit.
