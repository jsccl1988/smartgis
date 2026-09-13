<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task 6: Print preview (`smartgis.print`)

Work on `master` in `C:\Dev\src\gis\smartgis`. Do NOT git commit, branch, or push.

## Own these paths ONLY

- `src/plugin/print/print_preview_dialog.h` `.cc`
- `src/plugin/print/print_commands.h` `.cc`
- Add `source_set("print_views")` to `src/plugin/print/BUILD.gn` (keep leftover DLL).

## Do NOT touch

`host_test.cc`, `src/plugin/BUILD.gn`, leftover `dlg_2d_xview.*`, other domains, views toolkit, widgets sources (you may `#include` them).

## APIs

```cpp
namespace plugin {
bool register_print(content::PluginHost* host);
}
```

`PrintPreviewDialog` is a shell: `plugin::MapPreviewView` + Save `ui::views::Button`. Save uses `ui::views::pick_save_file`. Do **not** copy leftover `CDlg2DXView`. Do not add a second preview type.

`register_print` contributes `print.preview` → dialog `print.preview`. Handler only `open_dialog`.

## GN

```gn
source_set("print_views") {
  sources = [
    "print_commands.cc", "print_commands.h",
    "print_preview_dialog.cc", "print_preview_dialog.h",
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

Copyright 2026. `plugin` namespace. `snake_case`. English comments. No Qt.

Compile: `ninja -C out src/plugin/print:print_views`. Fix until green.

Report: `.superpowers/sdd/task-6-report.md`. No commit.
