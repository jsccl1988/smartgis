// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Views 3D tab orbit / wheel browse. Do not select_map_tab(2) here:
// a second select remounts FlyCube DXGI onto a navy clear.

script "browse.3d" {
  seq @inproc {
    pump(300);
    require_hwnd();
    mark("show");
    mark("hwnd-ok");
    mark("suite-browse3d");
    pump(200);
    // 3D needs china land rings for the atmosphere sea-mask.
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    pump(200);

    pump(400);
    drag(map_client, 80, 80, 160, 120);
    pump(200);
    drag(map_client, 160, 120, 100, 160);
    mark("browse3d-orbit-ok");

    wheel(map_client, 120, 120, -120);
    pump(100);
    wheel(map_client, 120, 120, 120);
    mark("browse3d-wheel-ok");

    // Capture while present threads still paint. Stopping timers first
    // yields a hollow navy FlyCube frame.
    pump(150);
    capture_browse_still("scene3d");
    mark("browse-exit");
    stop_map_timers();
  }
}
