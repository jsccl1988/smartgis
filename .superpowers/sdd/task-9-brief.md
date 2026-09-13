<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 9: baogrid commands (`smartgis.baogrid`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

- `src/plugin/baogrid/baogrid_commands.h` `.cc`
- Add `source_set("baogrid_views")` to `src/plugin/baogrid/BUILD.gn` (keep leftover DLL).

## Do NOT touch

`host_test.cc`, `src/plugin/BUILD.gn`, leftover plugin `.cpp`, other domains.

## APIs

```cpp
namespace plugin {
bool register_baogrid(content::PluginHost* host);
}
```

Four command ids:

| id | behavior |
| --- | --- |
| `baogrid.input_boundary_0` | `host->execute("edit.append.linestring", args)` if workspace catalog attached; else false |
| `baogrid.input_boundary_2` | same |
| `baogrid.save_boundary` | leftover body was empty → return **false** without crashing |
| `baogrid.load_boundary` | `ui::views::pick_open_file` |

Processing: `baogrid.create_orth_grid` — factory may call leftover `SmtBAOrthGrid` (`src/algorithm/baogrid/baorthgrid.h`) or stub `false` if that pulls MFC.

No new `CDialog`. File picker + message box only.

## GN

```gn
source_set("baogrid_views") {
  sources = [ "baogrid_commands.cc", "baogrid_commands.h" ]
  include_dirs += [ "//src" ]
  deps = [
    "//src/content:content",
    "//src/plugin:host",
    "//src/ui/views:views",
  ]
}
```

Copyright 2026. `plugin` namespace. `snake_case`. English comments. No Qt.

Compile: `ninja -C out src/plugin/baogrid:baogrid_views`. Fix until green.

Report: `.superpowers/sdd/task-9-report.md`. No commit.
