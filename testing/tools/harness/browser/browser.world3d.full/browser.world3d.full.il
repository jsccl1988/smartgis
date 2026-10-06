// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Full atmosphere. Same IR lanes as browser.world3d.browse (integration).
// Payload: world3d.scenario.atmosphere.full.

script "browser.world3d.full" {
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
    run_plugin_command("world3d.scenario.atmosphere.full");
    fit_scene_box();
    pump(ms=200);
    mark("fit-box-ok");
    camera_fly(mode="orbit", ms=1600, steps=20);
    mark("camera-fly-ok");
  }
}
