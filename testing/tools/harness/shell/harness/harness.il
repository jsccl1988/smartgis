// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// HWND / plugin scenario pipeline (suite "harness" / "self_test").
// Gate order lives here; C++ only supplies wait/expect/activate atoms.

script "harness" {
  seq @inproc {
    mark("show");
    pump(400);
    require_hwnd();
    mark("hwnd-ok");

    wait_viewport(face="map", ms=90000, frame=1);
    mark("map-frame-ok");
    mark("map-ready");
    mark("post-map");

    expect_shell_tree();
    mark("root-ok");
    mark("child-count-ok");
    mark("columns-ok");
    mark("catalog-ok");
    mark("data-ready");

    select_map_tab(1);
    pump(400);
    expect_scene_visible();
    wait_viewport(face="scene", ms=90000, frame=1);
    mark("scene-frame-ok");
    mark("scene-ready");
    mark("pre-orbit");

    activate_tool("view3d.trackball");
    expect_tool(id="view3d.trackball", fail_rc=23);
    mark("orbit-activated");
    drag(map_client, 24, 30, 48, 52);
    mark("orbit-dispatched");
    expect_orbit_moved();
    mark("orbit-ok");

    select_map_tab(0);
    pump(100);

    run_plugin_command("map2d.scenario.edit_m0");
    run_plugin_command("map2d.scenario.layers_m1");
    run_plugin_command("map2d.scenario.navigate");
    run_plugin_command("map2d.scenario.present");

    mark("layout-checked");
    expect_layout_bounds();
    expect_map_hwnd_sync();
    mark("map-bounds-ok");
    mark("hwnd-sync-ok");

    run_plugin_command("map2d.scenario.milestones");
  }
}
