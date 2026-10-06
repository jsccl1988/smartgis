// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Scene3D tab chrome BMP (suite "ui.scene").

script "ui.scene" {
  seq @inproc {
    mark("scene");
    apply_ui_theme();
    mark("theme-ok");
    mark("pump-pre");
    pump(600);
    mark("pump-post");
    require_hwnd();
    mark("hwnd-ok");
    mark("interact-pre");
    apply_scenario_panels("scene");
    mark("interact-post");
    layout_gate("scene");
    ui_present_capture("scene");
  }
}
