// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Mine Scene3D integration (marks + optional BMP from C++ body).
// IR: horizon HWND → view scene → plugin mine.scenario.showcase.

script "plugin.mine" {
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
    // C++ body frames the pad, orbits the lithology cube, and marks
    // fit-box-ok / camera-fly-ok. Post-showcase IL fit_scene_box / camera_fly
    // has AV'd FlyCube ExitProcess after a live overlay present.
    run_plugin_command("mine.scenario.showcase");
  }
}
