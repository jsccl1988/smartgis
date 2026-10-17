// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Plugins dock only. No Report / open_report side effects.
// Tab index matches shell_layout_composer: Tools=0 … Atmosphere=11 Plugins=12.

script "ui.plugins" {
  seq @inproc {
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);
    require_plugins();
    mark("plugins-ok");
    inspector_tab(index=12);
    pump(ms=400);
    mark("plugins-tab-ok");
    mark("pass");
  }
}
