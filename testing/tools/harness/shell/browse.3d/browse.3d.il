// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Views 3D tab orbit / wheel browse (forensic marks + optional record).

script "browse.3d" {
  seq @inproc {
    // C++ already selected Map tab 2 + lingered FlyCube. A second
    // select_map_tab(2) ExitProcess(-1) after dsl-load-ok.
    pump(ms=400);

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
