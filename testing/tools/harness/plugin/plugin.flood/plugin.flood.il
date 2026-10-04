// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin flood.inundate → inundation mask viz + frame export.

script "plugin.flood" {
  seq @inproc {
    require_plugins();
    resolve_data(kind="plugin", leaf="flood_basin_sample.tif", as="dem");
    require_var(name="dem", fail_mark="flood-sample-fail");
    sidecar_path(rel="..\\data\\plugin\\flood_mask.tif", as="out");
    require_var(name="out", fail_mark="flood-out-fail");
    doc_clear();
    run_processing(
      id="flood.inundate",
      args="{\"dem\":\"$dem\",\"output\":\"$out\",\"seed_x\":114.28,\"seed_y\":30.52,\"water_level\":52.0,\"frames\":8}"
    );
    mark("flood-ok");
    fit_extent();
    pump(ms=400);
    invalidate_map2d();
    pump(ms=200);
    export_bmp(leaf="plugin-showcase-flood.bmp", frame="flood_wuhan");
    mark("bmp-ok");
    // Live BMP is the gate (peer plugin.traffic).
    mark("playback-ok");
    mark("pass");
    detach_maps();
  }
}
