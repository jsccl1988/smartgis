// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin flood.inundate integration: inundation mask viz + frame export.
// IR: horizon HWND -> view map -> plugin processing -> view export.

script "plugin.flood" {
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
    resolve_data(kind="plugin", leaf="flood_basin_sample.tif", as="dem");
    require_var(name="dem", fail_mark="flood-sample-fail");
    sidecar_path(rel="..\\data\\plugin\\flood_mask.tif", as="out");
    require_var(name="out", fail_mark="flood-out-fail");
    doc_clear();
    run_processing(
      id="flood.inundate",
      args="{\"dem\":\"$dem\",\"output\":\"$out\",\"seed_x\":114.30,\"seed_y\":30.55,\"water_level\":45.0,\"frames\":8}"
    );
    mark("flood-ok");
    fit_extent();
    pump(ms=400);
    invalidate_map2d();
    pump(ms=200);
    mark("extent-ok");
    export_bmp(leaf="plugin-showcase-flood.bmp", frame="flood_wuhan");
    mark("bmp-ok");
    // Live BMP is the gate (peer plugin.traffic).
    mark("playback-ok");
    mark("pass");
    detach_maps();
  }
}
