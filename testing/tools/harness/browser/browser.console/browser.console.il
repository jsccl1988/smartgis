// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// DebugAgent console gate (:help / :layers / :extent / :refresh + pan bench).
// IR lanes: horizon (HWND / debug) -> document (china) -> view (tool / bench).

script "browser.console" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- view: soft map pane (frame optional) ---
    wait_viewport(face="map", ms=20000, frame=0);
    mark("map-ready");
    stop_map_timers();

    // --- horizon: DebugAgent ---
    wire_debug_agent();

    // --- document: china extent for :layers / :extent ---
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china", fail_rc=26);
    fit_extent();
    pump(ms=100);

    debug_exec(line=":help", contains=":layers", fail_rc=51);
    debug_exec(line=":layers", reject="no host.layer_names", fail_rc=52);
    debug_exec(line=":extent", reject="empty", fail_rc=53);
    debug_exec(line=":refresh", equals="refreshed", fail_rc=54);
    mark("console-ok");

    // --- view: pan bench ---
    tool(id="view.pan", fail_rc=40);
    console_pan_bench();
    mark("console-bench-ok");
    mark("pass");
    detach_maps();
    mark("detached");
  }
}
