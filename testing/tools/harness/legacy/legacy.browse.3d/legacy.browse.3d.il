// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Legacy scene3d showcase HWND: orbit / path / wheel during linger.
// Prefer modest orbit + zoom-in so china DEM stays framed (near_black gate).

script "legacy.browse.3d" {
  seq @os {
    window(activate);
    pump(ms=400);

    pan_burst(count=4, x=220, y=210, dx=14, dy=8, pump_ms=110);
    pump(ms=180);
    pan_burst(count=4, x=260, y=220, dx=-12, dy=-6, pump_ms=110);
    pump(ms=180);

    path([190, 190], [240, 205], [280, 200], [260, 230]);
    pump(ms=220);
    path([260, 230], [220, 215], [180, 205]);
    pump(ms=220);

    // Zoom in (positive delta) before any zoom-out so terrain stays framed.
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
