<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 8: model3d commands (`smartgis.model3d`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

- `src/plugin/model3d/model3d_commands.h` `.cc`
- Add `source_set("model3d_views")` to `src/plugin/model3d/BUILD.gn` (keep leftover DLL).

## Do NOT touch

`host_test.cc`, `src/plugin/BUILD.gn`, leftover plugin `.cpp`, other domains.

## APIs

```cpp
namespace plugin {
bool register_model3d(content::PluginHost* host);
}
```

Nine command ids (leftover `SMT_MSG_3DMODELCREATER_1`–`9`):

| id |
| --- |
| `model3d.add_pointcloud` |
| `model3d.add_sphere` |
| `model3d.add_water` |
| `model3d.add_terrain_grid` |
| `model3d.add_terrain_tin` |
| `model3d.create_tin` |
| `model3d.layer_points_to_3d` |
| `model3d.layer_lines_to_3d` |
| `model3d.layer_polygons_to_3d` |

Point-cloud / file actions use `ui::views::pick_open_file`. Errors use `ui::views::show_message_box`. Scene mutations only in `model3d_commands.cc` (or `run_processing` for `model3d.build_tin`). Do **not** add a new `CDialog`. Do **not** include leftover scene headers if they break the Views source_set — handlers must return `false` without crashing (no scene in tests).

`register_model3d` must succeed; `execute("model3d.add_sphere")` may return false.

## GN

```gn
source_set("model3d_views") {
  sources = [ "model3d_commands.cc", "model3d_commands.h" ]
  include_dirs += [ "//src" ]
  deps = [
    "//src/content:content",
    "//src/plugin:host",
    "//src/ui/views:views",
  ]
}
```

Copyright 2026. `plugin` namespace. `snake_case`. English comments. No Qt.

Compile: `ninja -C out src/plugin/model3d:model3d_views`. Fix until green.

Report: `.superpowers/sdd/task-8-report.md`. No commit.
