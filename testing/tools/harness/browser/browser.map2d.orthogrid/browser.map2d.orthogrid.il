// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Orthogrid map2d showcase. Atomic IL: map2d.seed(mode=orthogrid) + export_bmp.
// Frame: map2d_orthogrid. C++ fallback: map2d.scenario.orthogrid when IL off.

script "browser.map2d.orthogrid" {
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

    // --- plugin: seed ---
    require_plugins();
    select_map_tab(0);
    pump(ms=400);
    run_processing(
      id="map2d.seed",
      args="{\"mode\":\"orthogrid\"}"
    );
    mark("orthogrid-ok");
    mark("map2d-seed-plugin");

    // --- view: frame export ---
    stop_map_timers();
    export_bmp(leaf="map2d-showcase-orthogrid.bmp", frame="map2d_orthogrid");
    mark("fit-ok");
    mark("extent-ok");
    mark("pass");
    detach_maps();
  }
}
