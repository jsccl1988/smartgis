// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Map2d browse stress (integration). Same IR lanes as browser.map2d.* showcases.. Step order is IR-only (horizon / document / view).
// Optional forensic record: HARNESS_RECORD=1 on the suite / shell.

script "browser.map2d.browse" {
  seq @inproc {
    // --- horizon ---
    pump(ms=300);
    require_hwnd();
    mark("show");
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- document: china after Map tab is active ---
    // Seeding before select_map_tab(0) ExitProcess under FlyCube.
    select_map_tab(0);
    pump(ms=400);
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    pump(ms=200);

    // --- view: still + pan + stress + wheel + fps ---
    wait_viewport(face="map", ms=30000, frame=0);
    // Pause present before software export - concurrent GPU present and
    // MapIR rebuild under china_city ExitProcess mid export.
    stop_map_timers();
    capture_browse_still(face="map2d");
    resume_map_timers();
    pump(ms=100);

    wait_ready(ms=5000, soft=1);
    tool(id="view.pan");
    expect_tool(id="view.pan");
    drag(map_client, 40, 40, 70, 55);
    mark("pan-ok");

    // Keep present timers alive through stress so HWND BitBlt / motion_gate
    // sees carto change (ContentMapView + FORCE_GDI).
    mark("browse-pre-stress");
    // 32 large pans × ~45ms paint settle ->enough unique HWND frames for
    // motion_gate (min_unique_frames=8) under client_bitblt record.
    browse_stress(count=32);
    mark("browse-ok");

    expect_wheel_cursor(x=40, y=40);
    mark("wheel-cursor-ok");

    fps_bench();
    mark("browse-exit");
    mark("pass");
    // Do not pump after this. DispatchMessage can re-enter china MapIR.
    stop_map_timers();
    detach_maps();
  }
}
