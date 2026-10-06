// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// 2D gestures → resize → 3D gestures → resize → 2D gestures.

script "ui.interact" {
  seq @inproc {
    select_map_tab(0);
    // Do not wait_ready here: Map2d layout_build_count wait pumps present and
    // has AVd / ExitProcess(-1) under python-launched interact before Phase A.
    pump(800);
  }

  window(activate);

  // --- Phase A: 2D pan / wheel ---
  seq @inproc {
    tool("view.pan");
  }
  seq {
    click(map_client, 400, 360, left);
    pump(80);
    pan_burst(map_client, count=6, x=240, y=280, dx=40, dy=24, pump_ms=40);
    wheel_burst(map_client, count=3, x=400, y=400, delta=-120, pump_ms=40);
  }
  mark("interact-2d-a-ok") @inproc;

  // --- Phase A2: shrink shell on Map tab ---
  seq {
    window(resize, w=960, h=640);
    pump(400);
  }
  mark("interact-resize-2d-ok") @inproc;

  // Restore a workable client before FlyCube lazy-attach. Leaving 960x640 with
  // Diagnostic/Catalog seeds crushed Map/3D to ~210x93 and Init'd a stub
  // swapchain (heap thrash on Phase C). Resize-on-3D remains Phase B2.
  seq {
    window(resize, w=1280, h=800);
    pump(350);
  }
  mark("interact-pre-3d") @inproc;

  // --- Phase B: 3D tab (China DEM) orbit / wheel / WASD ---
  seq @inproc {
    select_map_tab(1);
  }
  mark("interact-3d-tab") @inproc;
  seq {
    // FlyCube product HWND settle — keep short to avoid atmosphere seed races.
    pump(600);
  }
  mark("interact-3d-pump") @inproc;
  seq {
    // Continuous orbit polyline (hand-feel depth).
    drag(map_client, 80, 80, 180, 130);
    pump(100);
    drag(map_client, 180, 130, 110, 170);
    pump(100);
    // Zoom in/out around cursor.
    wheel_burst(map_client, count=2, x=200, y=200, delta=-120, pump_ms=40);
    pump(80);
    wheel_burst(map_client, count=2, x=200, y=200, delta=120, pump_ms=40);
    pump(100);
    // WASD orbit (HUD contract).
    key(vk="W");
    pump(60);
    key(vk="A");
    pump(60);
    key(vk="S");
    pump(60);
    key(vk="D");
    pump(120);
  }
  mark("interact-3d-gestures") @inproc;
  export_bmp("ui-showcase-interact-3d.bmp", "scene3d") @inproc;
  mark("interact-3d-ok") @inproc;

  // --- Phase B2: one enlarge on 3D then restore (present paused in bind_host) ---
  seq @inproc {
    window(resize, w=1400, h=880);
    pump(200);
    window(resize, w=1280, h=800);
    pump(250);
  }
  mark("interact-resize-3d-ok") @inproc;

  // --- Phase C: back to 2D (shell already 1280×800) ---
  mark("interact-3d-to-2d-begin") @inproc;
  seq @inproc {
    select_map_tab(0);
    pump(500);
  }
  mark("interact-3d-to-2d-tab") @inproc;
  mark("interact-resize-restore-ok") @inproc;
  seq @inproc {
    tool("view.pan");
    pump(120);
  }
  seq {
    pan_burst(map_client, count=3, x=280, y=300, dx=28, dy=16, pump_ms=50);
    wheel_burst(map_client, count=2, x=400, y=400, delta=120, pump_ms=50);
    pump(200);
  }
  mark("interact-2d-b-ok") @inproc;
  // Reframe china before end-shell BMP — Phase C pan/wheel can leave a
  // tiny empty extent that fails interact_map_land_frac.
  seq @inproc {
    fit_extent();
    pump(200);
  }
  seq {
    wheel_burst(map_client, count=2, x=400, y=400, delta=-120, pump_ms=40);
    pump(200);
  }
  mark("interact-combo-ok") @inproc;
  mark("interact-script-ok") @inproc;
}
