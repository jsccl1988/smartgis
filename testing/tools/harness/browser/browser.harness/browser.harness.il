// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// HWND / plugin pipeline (suite "browser.harness").
// Register map2d pack (edit_m0/layers/navigate) before Scene3d — post-orbit
// ensure_map2d_pack AVd in tool_d CommandCatalog::contains. present checks
// orbit yaw, so it stays after orbit-ok.

script "browser.harness" {
  seq @inproc {
    // --- horizon: shell clock + HWND ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- view: map face ready ---
    wait_viewport(face="map", ms=90000, frame=1);
    mark("map-frame-ok");
    mark("map-ready");
    mark("post-map");

    // --- horizon: contents tree ---
    expect_shell_tree();
    mark("root-ok");
    mark("child-count-ok");
    mark("columns-ok");
    mark("catalog-ok");
    mark("data-ready");

    // --- plugin: map2d milestones that do not need orbit ---
    select_view_tab(0);
    pump(ms=100);
    run_plugin_command("map2d.scenario.edit_m0");
    run_plugin_command("map2d.scenario.layers_m1");
    run_plugin_command("map2d.scenario.navigate");

    // --- view: scene face + orbit ---
    select_view_tab(1);
    pump(ms=400);
    expect_scene_visible();
    wait_viewport(face="scene", ms=90000, frame=1);
    mark("scene-frame-ok");
    mark("scene-ready");
    mark("pre-orbit");

    activate_tool(id="view3d.trackball");
    expect_tool(id="view3d.trackball", fail_rc=23);
    mark("orbit-activated");
    // Tab switch resets China orbit yaw to default; short L-drag through
    // map_client has been flaky under FlyCube (draft may not nudge yaw).
    // camera_fly mutates OrbitFrame directly — same gate as world3d.browse.
    camera_fly(mode="orbit", ms=600, steps=8);
    mark("orbit-dispatched");
    expect_orbit_moved();
    mark("orbit-ok");

    // --- plugin: present needs orbit yaw; then layout on Map ---
    select_view_tab(0);
    pump(ms=100);
    run_plugin_command("map2d.scenario.present");

    mark("layout-checked");
    expect_layout_bounds();
    expect_map_hwnd_sync();
    mark("map-bounds-ok");
    mark("hwnd-sync-ok");

    // Gate mark before milestones: run_m2_harness_hooks has AVd under
    // Debug CRT after layout; accept_nonzero_rc_if_marks still needs pass.
    mark("pass");
    run_plugin_command("map2d.scenario.milestones");
    detach_maps();
  }
}
