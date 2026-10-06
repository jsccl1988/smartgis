// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Browse suite body. Step order lives here; C++ only fills Host slots.
// Optional forensic record: set HARNESS_RECORD=1 on the suite / shell.

script "browse" {
  seq @inproc {
    pump(300);
    require_hwnd();
    mark("show");
    mark("hwnd-ok");

    // Open china after the 2D tab is active. Seeding before select_map_tab(0)
    // ExitProcess under FlyCube (marks stopped at china-seed-ok).
    select_map_tab(0);
    pump(400);
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    pump(200);
    // Pause present before software export — concurrent GPU present and
    // MapIR rebuild under china_city ExitProcess mid export.
    stop_map_timers();
    capture_browse_still("map2d");
    resume_map_timers();
    pump(100);

    wait_ready(ms=5000, soft=1);
    tool("view.pan");
    expect_tool("view.pan");
    drag(map_client, 40, 40, 70, 55);
    mark("pan-ok");

    // Keep present timers alive through stress so HWND BitBlt / motion_gate
    // sees carto change (ContentMapView + FORCE_GDI).
    mark("browse-pre-stress");
    // 32 large pans × ~45ms paint settle → enough unique HWND frames for
    // motion_gate (min_unique_frames=8) under client_bitblt record.
    browse_stress(count=32);
    mark("browse-ok");

    expect_wheel_cursor(40, 40);
    mark("wheel-cursor-ok");

    fps_bench();
    mark("browse-exit");
    // Do not pump after this. DispatchMessage can re-enter china MapIR.
    stop_map_timers();
  }
}
