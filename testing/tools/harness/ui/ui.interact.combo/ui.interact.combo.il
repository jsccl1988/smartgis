// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Mid-weight gesture body: continuous combo ops (path / chord / bursts).
// Runs under ui.interact.host chrome (same entry as ui.interact / .os / .smoke).

script "ui.interact.combo" {
  window(activate);
  pump(80);

  // Pointer path + chord + bursts (shell_client keeps the DSL demo stable).
  seq {
    path(shell_client, [240, 300], [280, 320], [320, 310], [360, 330]);
    pump(40);
    chord(CTRL) {
      click(shell_client, 480, 360);
    }
    pump(40);
    pan_burst(shell_client, count=3, x=260, y=300, dx=28, dy=16, pump_ms=25);
    wheel_burst(shell_client, count=2, x=400, y=400, delta=-120, pump_ms=30);
  }

  key(ESCAPE);
  mark("interact-combo-demo-ok") @inproc;
}
