// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Input showcase digitize path (marks gate for suite "input").

script "input" {
  seq @inproc {
    mark("show");
    pump(300);
    require_hwnd();
    mark("hwnd-ok");

    select_map_tab(0);
    pump(400);
    wait_ready(ms=5000, soft=1);
    mark("map-ready");

    require_edit_host();
    mark("host-ok");

    // Point
    tool("edit.append.point");
    expect_tool("draw.point");
    click(map_client, 12, 18);
    expect_geom(point, min_points=1);
    mark("input-point-ok");

    // Linestring: two verts + RMB finish
    tool("edit.append.linestring");
    expect_tool("draw.linestring");
    click(map_client, 20, 20);
    click(map_client, 80, 60);
    rclick(map_client, 80, 60);
    expect_geom(linestring, min_points=2);
    mark("input-line-ok");

    // Polygon: three verts + RMB finish
    tool("edit.append.polygon");
    expect_tool("draw.polygon");
    click(map_client, 30, 30);
    click(map_client, 90, 30);
    click(map_client, 60, 80);
    rclick(map_client, 60, 80);
    expect_geom(polygon, min_points=3);
    mark("input-poly-ok");
    mark("input-ok");
    mark("pass");
  }
}
