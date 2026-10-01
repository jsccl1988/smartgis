// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Legacy SmartGis Edit: OS inject pan / path / wheel / corner drag on map_client.
// wheel_burst is unidirectional (every tick uses the given delta).
// Zoom-out runs after a short pan warmup so china carto has painted (zoom_gate).

script "legacy.browse.2d" {
  seq @os {
    window(activate);
    // DelayInit china_city + Edit MDI first paint.
    pump(ms=4000);

    // Warmup pan: forces map_client paint before zoom_gate sidecars.
    pan_burst(map_client, count=4, x=280, y=220, dx=18, dy=10, pump_ms=55);
    // Settle china carto before zoom sidecars (avoid ocean-only before BMP).
    pump(ms=900);

    // Click / select smoke (map_client client coords).
    click(map_client, 400, 300);
    pump(ms=80);
    click(map_client, 360, 260);
    pump(ms=80);

    // Unidirectional zoom-out (runner captures before/after for zoom_gate).
    wheel_burst(map_client, count=8, x=400, y=300, delta=-120, pump_ms=70);
    pump(ms=150);

    pan_burst(map_client, count=10, x=210, y=170, dx=32, dy=18, pump_ms=50);
    pump(ms=100);
    pan_burst(map_client, count=10, x=300, y=240, dx=-28, dy=-16, pump_ms=50);
    pump(ms=100);

    path(map_client, [160, 120], [220, 150], [280, 140], [340, 180], [380, 220], [300, 260]);
    pump(ms=140);
    path(map_client, [300, 260], [240, 230], [180, 250], [140, 190], [200, 150]);
    pump(ms=140);

    // Unidirectional zoom-in (asymmetric vs zoom-out).
    wheel_burst(map_client, count=7, x=400, y=300, delta=120, pump_ms=65);
    pump(ms=100);

    drag(map_client, 100, 90, 170, 140);
    pump(ms=70);
    drag(map_client, 500, 380, 430, 320);
    pump(ms=70);
    drag(map_client, 240, 200, 280, 160);
    pump(ms=80);
    pan_burst(map_client, count=5, x=230, y=190, dx=14, dy=-10, pump_ms=45);
    pump(ms=120);
    click(map_client, 280, 210);
    pump(ms=80);
    dblclick(map_client, 320, 240);
    pump(ms=200);
  }
}
