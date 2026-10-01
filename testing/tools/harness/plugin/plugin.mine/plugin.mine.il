// Copyright (c) 2026 The Mogu Authors.

// All rights reserved.

// Product plugin mine — Scene3D body lives in plugin_showcase.cc

// (--plugin-showcase=mine). This IL is retained for CapabilityHost dry-runs /

// documentation; the suite argv path uses the C++ Scene3D capture.



script "plugin.mine" {

  seq @inproc {

    resolve_data(kind="plugin", leaf="mine_boreholes.csv", as="csv");

    require_var(name="csv", fail_mark="mine-sample-fail");

    sidecar_path(rel="..\\data\\plugin\\mine_stratum_tin.json", as="out");

    require_var(name="out", fail_mark="mine-out-fail");

    doc_clear();

    run_processing(

      id="mine.interpolate_stratum",

      args="{\"input\":\"$csv\",\"stratum_id\":\"clay\",\"output\":\"$out\"}"

    );

    mark("mine-ok");

    fit_extent();

    pump(ms=300);

    run_processing(

      id="mine.prism_volume",

      args="{\"input\":\"$csv\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\"}"

    );

    mark("prism-ok");

    pump(ms=200);

    // Map2d export kept for IL-only runs; suite BMP comes from Scene3D HWND.

    export_bmp(leaf="plugin-showcase-mine.bmp", frame="mine_boreholes");

    mark("bmp-ok");

    mark("pass");

    detach_maps();

  }

}

