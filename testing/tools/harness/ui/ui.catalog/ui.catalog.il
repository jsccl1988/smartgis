// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Catalog Maps page chrome BMP (suite "ui.catalog").

script "ui.catalog" {
  seq @inproc {
    mark("catalog");
    apply_ui_theme();
    mark("theme-ok");
    mark("pump-pre");
    pump(600);
    mark("pump-post");
    require_hwnd();
    mark("hwnd-ok");
    mark("interact-pre");
    apply_scenario_panels("catalog");
    // Pin Maps — seed leaves Layers active (visual_review #3).
    catalog_tab(index=2);
    pump(ms=200);
    mark("catalog-maps");
    mark("interact-post");
    layout_gate("catalog");
    catalog_tab(index=2);
    pump(ms=120);
    ui_present_capture("catalog");
  }
}
