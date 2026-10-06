// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Shell chrome BMP path (suite "ui.shell").

script "ui.shell" {
  seq @inproc {
    mark("shell");
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
    apply_scenario_panels("shell");
    mark("interact-post");
    layout_gate("shell");
    ui_present_capture("shell");
  }
}
