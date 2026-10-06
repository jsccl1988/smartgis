// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// StyleDocument align showcase. Same IR lanes as browser.map2d.browse.
// Payload: map2d.scenario.align (smartgis.map2d).

script "browser.map2d.align" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- view: map face ---
    wait_viewport(face="map", ms=30000, frame=0);
    mark("map-ready");

    // --- plugin ---
    require_plugins();
    run_plugin_command("map2d.scenario.align");
    fit_extent();
    pump(ms=300);
    invalidate_map2d();
    pump(ms=200);
    mark("extent-ok");
  }
}
