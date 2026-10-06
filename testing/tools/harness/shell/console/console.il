// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// DebugAgent console self-test (:help/:layers/:extent/:refresh + pan bench).

script "console" {
  seq @inproc {
    mark("show");
    pump(400);
    require_hwnd();
    mark("hwnd-ok");

    wait_viewport(face="map", ms=20000, frame=0);
    mark("map-ready");
    stop_map_timers();

    wire_debug_agent();
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china", fail_rc=26);
    fit_extent();
    pump(100);

    debug_exec(line=":help", contains=":layers", fail_rc=51);
    debug_exec(line=":layers", reject="no host.layer_names", fail_rc=52);
    debug_exec(line=":extent", reject="empty", fail_rc=53);
    debug_exec(line=":refresh", equals="refreshed", fail_rc=54);
    mark("console-ok");

    tool(id="view.pan", fail_rc=40);
    console_pan_bench();
    mark("console-bench-ok");

    detach_maps();
    mark("detached");
  }
}
