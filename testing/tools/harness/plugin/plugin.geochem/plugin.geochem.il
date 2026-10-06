// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin geochem.analyze integration → graded samples + IDW + BMP.
// IR: horizon HWND → view map → plugin processing → view export.

script "plugin.geochem" {
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
    resolve_data(kind="plugin", leaf="geochem_samples.csv", as="csv");
    require_var(name="csv", fail_mark="geochem-sample-fail");
    sidecar_path(rel="..\\data\\plugin\\geochem_idw.tif", as="out");
    require_var(name="out", fail_mark="geochem-out-fail");
    doc_clear();
    run_processing(
      id="geochem.analyze",
      args="{\"input\":\"$csv\",\"element\":\"Cu\",\"correlate\":\"Au\",\"cells\":32,\"classes\":5,\"output\":\"$out\"}"
    );
    mark("geochem-ok");
    fit_extent();
    pump(ms=300);
    run_processing(
      id="geochem.stats",
      args="{\"input\":\"$csv\",\"element\":\"Cu\",\"correlate\":\"Au\",\"bins\":8}"
    );
    mark("stats-ok");
    pump(ms=200);
    fit_extent();
    invalidate_map2d();
    pump(ms=200);
    mark("extent-ok");
    export_bmp(leaf="plugin-showcase-geochem.bmp", frame="geochem_samples");
    mark("bmp-ok");
    mark("pass");
    detach_maps();
  }
}
