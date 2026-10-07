// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// StyleDocument align showcase. Atomic IL: map2d.seed(mode=align) + export_bmp.
// Frame: map2d_china. C++ fallback: map2d.scenario.align when IL off.

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

    // --- plugin: seed ---
    require_plugins();
    select_map_tab(0);
    pump(ms=400);
    stop_map_timers();
    run_processing(
      id="map2d.seed",
      args="{\"mode\":\"align\"}"
    );
    mark("china-ok");
    mark("map2d-seed-plugin");
    pump(ms=300);
    invalidate_map2d();
    pump(ms=200);

    // --- view: frame export ---
    export_bmp(leaf="map2d-showcase-align.bmp", frame="map2d_china");
    mark("fit-ok");
    mark("extent-ok");
    mark("pass");
    detach_maps();
  }
}
