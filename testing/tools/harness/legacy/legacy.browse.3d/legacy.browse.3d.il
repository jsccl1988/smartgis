// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Bare SmartGis.exe (no argv): Edit shell → 窗口/三维窗口 (ID_WND_3D=32915),
// then OS inject orbit / path / wheel on the 3D client.

script "legacy.browse.3d" {
  seq @os {
    window(activate);
    // DelayInit china_city + Edit MDI first paint.
    pump(ms=4000);

    // Open leftover 3D MDI (no --scene3d-showcase). Maximize settles after
    // open_mdi_view; extra pump lets GL seed + first present finish.
    wm_command(id=32915);
    pump(ms=4500);

    pan_burst(count=4, x=220, y=210, dx=14, dy=8, pump_ms=110);
    pump(ms=180);
    pan_burst(count=4, x=260, y=220, dx=-12, dy=-6, pump_ms=110);
    pump(ms=180);

    path([190, 190], [240, 205], [280, 200], [260, 230]);
    pump(ms=220);
    path([260, 230], [220, 215], [180, 205]);
    pump(ms=220);

    wheel_burst(count=3, x=230, y=220, delta=120, pump_ms=120);
    pump(ms=140);
    wheel_burst(count=1, x=230, y=220, delta=-120, pump_ms=120);
    pump(ms=180);

    drag(x0=200, y0=190, x1=240, y1=210);
    pump(ms=100);
    drag(x0=320, y0=240, x1=280, y1=220);
    pump(ms=250);
  }
}
