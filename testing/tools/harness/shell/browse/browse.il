// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Browse navigate stage (pan / browse stress / wheel-cursor).
// Optional forensic record: set SMT_HARNESS_RECORD=1 on the suite / shell.

script "browse" {
  seq @inproc {
    wait_ready(ms=5000, soft=1);
    tool("view.pan");
    expect_tool("view.pan");
    drag(map_client, 40, 40, 70, 55);
    mark("pan-ok");

    // Keep present timers alive through stress so HWND BitBlt / motion_gate
    // sees carto change (ContentMapView + FORCE_GDI). AV race with WM_TIMER
    // was on FlyCube; product browse face is GDI overlay now.
    mark("browse-pre-stress");
    // 32 large pans × ~45ms paint settle → enough unique HWND frames for
    // motion_gate (min_unique_frames=8) under client_bitblt record.
    browse_stress(count=32);
    mark("browse-ok");

    expect_wheel_cursor(40, 40);
    mark("wheel-cursor-ok");

    // C++ showcase stops timers after dsl-done (no post-IL pump).
  }
}
