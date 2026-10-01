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

    // Kill present timers before pan/wheel bursts (AV race under concurrent
    // WM_TIMER present + synthetic input). Do not pump the full UI queue
    // here — re-entrant paint during stress is the failure mode.
    stop_map_timers();
    mark("browse-pre-stress");
    browse_stress(count=24);
    mark("browse-ok");

    expect_wheel_cursor(40, 40);
    mark("wheel-cursor-ok");

    // Do not stop_map_timers here — C++ showcase captures shell BMP while
    // present is still live, then stops timers before TerminateProcess.
  }
}
