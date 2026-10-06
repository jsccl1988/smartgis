// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Orthogrid3D hex volume integration (marks + optional BMP from C++ body).
// IR: horizon HWND → view scene → plugin orthogrid3d.scenario.showcase.

script "plugin.orthogrid3d" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- view: Scene3D face ---
    select_map_tab(1);
    pump(ms=400);
    wait_viewport(face="scene", ms=90000, frame=0);
    mark("scene-ready");

    // --- plugin ---
    require_plugins();
    run_plugin_command("orthogrid3d.scenario.showcase");
    fit_scene_box();
    pump(ms=200);
    mark("fit-box-ok");
    camera_fly(mode="orbit", ms=1800, steps=22);
    mark("camera-fly-ok");
  }
}
