// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin traffic.cost_path integration → map path viz + frame export.
// IR: horizon HWND → view map → plugin processing → view export.

script "plugin.traffic" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- view: map face ---
    wait_viewport(face="map", ms=30000, frame=0);
    mark("map-ready");

    // --- plugin ---
    require_plugins();
    resolve_data(kind="plugin", leaf="traffic_network_sample.geojson", as="net");
    require_var(name="net", fail_mark="traffic-sample-fail");
    sidecar_path(rel="..\\data\\plugin\\traffic_path.geojson", as="out");
    require_var(name="out", fail_mark="traffic-out-fail");
    doc_clear();
    run_processing(
      id="traffic.cost_path",
      args="{\"network\":\"$net\",\"output\":\"$out\",\"start_x\":116.335,\"start_y\":39.870,\"end_x\":116.452,\"end_y\":39.9285,\"weight_field\":\"cost\",\"frames\":12}"
    );
    mark("traffic-ok");
    fit_extent();
    pump(ms=400);
    invalidate_map2d();
    pump(ms=200);
    mark("extent-ok");
    export_bmp(leaf="plugin-showcase-traffic.bmp", frame="traffic_beijing");
    mark("bmp-ok");
    // Live BMP after commit is the harness gate (peer plugin.flood).
    mark("playback-ok");
    mark("pass");
    detach_maps();
  }
}
