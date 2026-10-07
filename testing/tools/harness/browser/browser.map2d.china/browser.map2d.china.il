// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// China carto showcase. Atomic IL: map2d.seed(mode=china) + export_bmp.
// Frame: china_product. C++ fallback: map2d.scenario.china when IL off.

script "browser.map2d.china" {
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
      args="{\"mode\":\"china\"}"
    );
    mark("china-ok");
    mark("map2d-seed-plugin");

    // --- view: frame export ---
    export_bmp(leaf="map2d-showcase-china.bmp", frame="china_product");
    mark("fit-ok");
    mark("extent-ok");
    mark("pass");
    stop_map_timers();
    detach_maps();
  }
}
