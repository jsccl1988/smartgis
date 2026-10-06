// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Shared chrome for ui.interact / .os / .smoke / .combo.
// apply_scenario_panels("interact") runs UI_INTERACT_GESTURE_SCRIPT (or
// harness ui.interact/ui.interact.il).

script "ui.interact.host" {
  seq @inproc {
    mark("interact");
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
    apply_scenario_panels("interact");
    pump(200);
    mark("interact-post");
    layout_gate("interact");
    ui_present_capture("interact");
  }
}
