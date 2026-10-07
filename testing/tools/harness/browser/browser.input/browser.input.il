// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Input digitize path (marks gate for suite "browser.input").
// IR: horizon HWND -> view map ready + edit host -> geom expects.

script "browser.input" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=300);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- document / view: map face for digitize ---
    select_map_tab(0);
    pump(ms=400);
    resolve_data(kind="data", leaf="china_city.gpkg", as="china");
    require_var(name="china", fail_mark="china-seed-miss");
    open_map(var="china");
    fit_extent();
    wait_viewport(face="map", ms=30000, frame=0);
    wait_ready(ms=5000, soft=1);
    mark("map-ready");

    require_edit_tool_session();
    mark("host-ok");

    // Point
    tool(id="edit.append.point");
    expect_tool(id="draw.point");
    click(map_client, 12, 18);
    expect_geom(kind="point", min_points=1);
    mark("input-point-ok");

    // Linestring: two verts + RMB finish
    tool(id="edit.append.linestring");
    expect_tool(id="draw.linestring");
    click(map_client, 20, 20);
    click(map_client, 80, 60);
    rclick(map_client, 80, 60);
    expect_geom(kind="linestring", min_points=2);
    mark("input-line-ok");

    // Polygon: three verts + RMB finish
    tool(id="edit.append.polygon");
    expect_tool(id="draw.polygon");
    click(map_client, 30, 30);
    click(map_client, 90, 30);
    click(map_client, 60, 80);
    rclick(map_client, 60, 80);
    expect_geom(kind="polygon", min_points=3);
    mark("input-poly-ok");
    mark("input-ok");
    mark("pass");
    detach_maps();
  }
}
