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
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    mark("interact-pre");
    apply_scenario_panels("catalog");
    mark("interact-post");
    layout_gate("catalog");
    ui_present_capture("catalog");
  }
}
