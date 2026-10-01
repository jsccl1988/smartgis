// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Views 3D tab orbit / wheel browse (forensic marks + optional record).

script "browse.3d" {
  seq @inproc {
    select_map_tab(2);
    // Lazy FlyCube + China orbit settle before input (avoid tool() during attach).
    pump(ms=1500);
    wait_ready(ms=8000, soft=1);

    // switch_map_tab(2) already activates view3d.trackball. Avoid tool() here:
    // run_tool_command after lazy Scene3D FlyCube attach has AVd on ui_.
    expect_tool("view3d.trackball");
    drag(map_client, 80, 80, 160, 120);
    pump(ms=200);
    drag(map_client, 160, 120, 100, 160);
    mark("browse3d-orbit-ok");

    wheel(map_client, 120, 120, -120);
    pump(ms=100);
    wheel(map_client, 120, 120, 120);
    mark("browse3d-wheel-ok");
  }
}
