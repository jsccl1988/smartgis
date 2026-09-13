<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 4: DEM Views (`smartgis.dem`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

Create (do not edit leftover `dlg_*.h` / `dlg_*.cpp` as the new API):

- `src/plugin/dem/tin_loader_dialog.h` `.cc`
- `src/plugin/dem/grid_loader_dialog.h` `.cc`
- `src/plugin/dem/dem_commands.h` `.cc`

Modify `src/plugin/dem/BUILD.gn` only by **adding** `source_set("dem_views")` next to the existing leftover `smt_mfc_shared_library`. Do not change leftover `dll_stem` or leftover source lists.

## Do NOT touch

`src/plugin/host_test.cc`, `src/plugin/BUILD.gn`, `src/plugin/manifest.*`, `src/plugin/registry.*`, `src/content/**`, `src/ui/views/**`, `src/plugin/widgets/**`, other domain trees.

## APIs to use (already landed)

- `content::PluginHost` in `content/public/plugin_host.h`
- `plugin::AboutDialog` in `plugin/widgets/about_dialog.h`
- Views: `ui/views/{label,button,textfield,checkbox,radio_button,combobox,tab_strip,table_view,file_picker,message_box}.h`

```cpp
namespace plugin {
bool register_dem(content::PluginHost* host);
}
```

`register_dem` contributes:

| command | title (leftover Chinese OK) | opens dialog |
| --- | --- | --- |
| `dem.load_tin` | 离散点生成DEM | `dem.tin_loader` |
| `dem.load_grid` | 高度图生成DEM | `dem.grid_loader` |
| `dem.about` | 关于 | `dem.about` |

Dialog factories: construct the Views types (About uses `plugin::AboutDialog`). Must not crash.

Processing: `dem.tin_from_xyz`, `dem.grid_from_heightmap`. Factories may call leftover `Smt3DTinLoader` / `Smt3DGridLoader` **only from dem_commands.cc**. If leftover headers pull MFC and break the Views source_set, stub the factory to return `false` (do not include leftover `dlg_*.h`). Dialog `.cc` must NOT include `tin.h` / GEOS / PROJ / leftover loaders.

## TinLoaderDialog fields (match leftover CDlgTinLoader)

vertex path, separator radios (tab/space/comma), head/line skip, XYZ column comboboxes, X/Y/Z scales, texture checkbox + path, generate-2D-TIN checkbox + layer combobox. OK builds a JSON object string and `host->run_processing("dem.tin_from_xyz", json)`.

## GridLoaderDialog fields (match leftover CDlgGridLoader)

heightmap path, X/Y/Z scales, X/Y/Z starts, texture checkbox + path, color type combobox. OK → `dem.grid_from_heightmap`.

## GN

```gn
source_set("dem_views") {
  sources = [
    "dem_commands.cc",
    "dem_commands.h",
    "grid_loader_dialog.cc",
    "grid_loader_dialog.h",
    "tin_loader_dialog.cc",
    "tin_loader_dialog.h",
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

Copyright `(c) 2026 The Mogu Authors` on new files. Namespaces `plugin` only (two levels). Functions `snake_case`. Comments English. C++23. No Qt. No new `*Manager`.

Compile: `ninja -C out src/plugin/dem:dem_views` (after `build.bat`/`gn gen out` if needed). Output only `out/`. Fix until green.

Write report to `.superpowers/sdd/task-4-report.md`. No commit.

**Status line only:** DONE / DONE_WITH_CONCERNS / BLOCKED + test/compile summary.
