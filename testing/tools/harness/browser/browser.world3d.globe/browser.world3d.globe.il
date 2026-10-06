// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Globe atmosphere. Same IR lanes as browser.world3d.browse (integration).
// Payload: world3d.scenario.atmosphere.globe.

script "browser.world3d.globe" {
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
    run_plugin_command("world3d.scenario.atmosphere.globe");
    // Model-box framing + spherical globe fly (space → skim).
    fit_scene_box();
    pump(ms=200);
    mark("fit-box-ok");
    camera_fly(mode="spherical", ms=2400, steps=24);
    mark("camera-fly-ok");
  }
}
