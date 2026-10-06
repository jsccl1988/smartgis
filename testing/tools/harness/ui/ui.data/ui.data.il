// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Map Data tab chrome BMP (suite "ui.data").

script "ui.data" {
  seq @inproc {
    mark("data");
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
    apply_scenario_panels("data");
    mark("interact-post");
    layout_gate("data");
    ui_present_capture("data");
  }
}
