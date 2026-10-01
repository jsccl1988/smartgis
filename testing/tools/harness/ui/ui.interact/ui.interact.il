// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Upgraded to Interact DSL — continuous chrome + map combo script.

script "ui.interact" {
  // Tab chrome tour (inproc semantic APIs only).
  seq @inproc {
    select_map_tab(0);
    pump(200);
    select_map_tab(1);
    pump(250);
    select_map_tab(2);
    pump(350);
    select_map_tab(0);
  }

  repeat 2 @inproc {
    catalog_tab(0);
    pump(80);
    catalog_tab(1);
    pump(80);
    catalog_tab(0);
  }

  window(activate);

  // Continuous pointer combo (both drivers).
  seq {
    click(shell_client, 640, 420, left);
    pump(60);
    path(shell_client, [220, 320], [260, 340], [300, 360], [340, 350]);
    pump(60);
    pan_burst(map_client, count=4, x=240, y=280, dx=36, dy=20, pump_ms=30);
    wheel_burst(map_client, count=3, x=400, y=400, delta=-120, pump_ms=40);
  }

  chord(CTRL) {
    click(shell_client, 500, 380);
  }

  repeat 2 {
    drag(shell_client, 200, 300, 260, 340);
    pump(40);
    wheel(shell_client, 400, 400, 120);
    pump(40);
  }

  key(ESCAPE);

  // Real system pointer sample (OS driver only).
  click(shell_client, 120, 120) @os @inject=sendinput;

  mark("interact-combo-ok") @inproc;
  mark("interact-script-ok") @inproc;
  select_map_tab(0) @inproc;
  pump(300);
}
