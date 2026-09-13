<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 5: Projection Views (`smartgis.proj`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

- `src/plugin/proj/map_prj_dialog.h` `.cc`
- `src/plugin/proj/map_prj_xy_page.h` `.cc`
- `src/plugin/proj/map_prj_grid_page.h` `.cc`
- `src/plugin/proj/proj_commands.h` `.cc`
- Add `source_set("proj_views")` to `src/plugin/proj/BUILD.gn` (keep leftover `smt_mfc_shared_library` / `dll_stem` unchanged).

## Do NOT touch

`host_test.cc`, `src/plugin/BUILD.gn`, other domain trees, `src/content/**`, `src/ui/views/**`, `src/plugin/widgets/**`.

## APIs

```cpp
namespace plugin {
bool register_proj(content::PluginHost* host);
}
```

- `MapPrjDialog` hosts `ui::views::TabStrip` with `MapPrjXyPage` and `MapPrjGridPage`.
- XY page fields (leftover `CDlgMapPrjDoXY`): L, B, X, Y, scale ruler. Apply → `host->run_processing("proj.transform_xy", json)`.
- Grid page fields (leftover `CDlgMapPrjDoGrid`): dL, dB, Lmin/Bmin/Lmax/Bmax, scale. Apply → `proj.transform_grid`.
- No PROJ headers in dialog `.cc`. Factory in `proj_commands.cc` may call `src/algorithm/proj` (`projection_api.h`) or stub `return false` if that pulls leftover MFC.

`register_proj` contributes command `proj.do_prj` (title leftover 投影变换 or `Do projection`) → dialog `proj.dialog`. Command handler only `open_dialog`.

## GN

```gn
source_set("proj_views") {
  sources = [
    "map_prj_dialog.cc", "map_prj_dialog.h",
    "map_prj_grid_page.cc", "map_prj_grid_page.h",
    "map_prj_xy_page.cc", "map_prj_xy_page.h",
    "proj_commands.cc", "proj_commands.h",
  ]
  include_dirs += [ "//src" ]
  deps = [
    "//src/content:content",
    "//src/plugin:host",
    "//src/ui/views:views",
  ]
}
```

Copyright 2026 Mogu Authors. `plugin` namespace. `snake_case`. English comments. No Qt.

Compile: `ninja -C out src/plugin/proj:proj_views`. Fix until green.

Report: `.superpowers/sdd/task-5-report.md`. No commit.
