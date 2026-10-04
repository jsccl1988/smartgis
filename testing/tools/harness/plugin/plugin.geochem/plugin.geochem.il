// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin geochem.analyze → graded samples + IDW heat raster + BMP.

script "plugin.geochem" {
  seq @inproc {
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
    export_bmp(leaf="plugin-showcase-geochem.bmp", frame="geochem_samples");
    mark("bmp-ok");
    mark("pass");
    detach_maps();
  }
}
