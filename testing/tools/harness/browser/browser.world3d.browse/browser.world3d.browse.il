// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// World3d browse stress (integration). Same IR lanes as browser.world3d.* showcases.
// IR: document china seed -> view scene face -> fit box / camera fly -> still.
// Do not select_map_tab again after capture (FlyCube remount ->navy clear).

script "browser.world3d.browse" {
  seq @inproc {
    // --- horizon ---
    pump(ms=300);
    require_hwnd();
    mark("show");
    mark("hwnd-ok");
    mark("suite-browse3d");
    suppress_dialogs(on=1);
    pump(ms=200);

    // --- document: china land rings for atmosphere sea-mask ---
    select_map_tab(0);
    pump(ms=200);
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    pump(ms=200);

    // --- view: scene face (index 1). One select only. ---
    select_map_tab(1);
    pump(ms=400);
    expect_scene_visible();
    wait_viewport(face="scene", ms=60000, frame=0);
    mark("scene-ready");

    // Best viewport from world-component AABB, then orbit tour.
    fit_scene_box();
    pump(ms=200);
    mark("fit-box-ok");
    camera_fly(mode="orbit", ms=1600, steps=20);
    expect_orbit_moved();
    mark("browse3d-orbit-ok");
    mark("camera-fly-ok");

    wheel(map_client, 120, 120, -120);
    pump(ms=100);
    wheel(map_client, 120, 120, 120);
    mark("browse3d-wheel-ok");

    // Capture while present threads still paint. Stopping timers first
    // yields a hollow navy FlyCube frame.
    pump(ms=150);
    capture_browse_still(face="scene3d");
    mark("browse-exit");
    mark("pass");
    stop_map_timers();
    detach_maps();
  }
}
