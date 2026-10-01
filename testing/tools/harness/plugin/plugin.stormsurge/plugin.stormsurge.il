// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin stormsurge.run → map2d inundation + playback (Wave 2 script).
// Scene3D HWND BMP lives in plugin_showcase.cc (run_stormsurge_scene3d);
// --plugin-showcase=stormsurge does not execute this body.

script "plugin.stormsurge" {
  seq @inproc {
    resolve_data(kind="plugin", leaf="stormsurge_dem_sample.tif", as="dem");
    require_var(name="dem", fail_mark="stormsurge-sample-fail");
    resolve_data(kind="plugin", leaf="stormsurge_coast_sample.geojson", as="coast");
    require_var(name="coast", fail_mark="stormsurge-coast-fail");
    sidecar_path(rel="..\\data\\plugin\\stormsurge_mask.tif", as="out");
    require_var(name="out", fail_mark="stormsurge-out-fail");
    doc_clear();
    run_processing(
      id="stormsurge.load_coast",
      args="{\"coast\":\"$coast\"}"
    );
    run_processing(
      id="stormsurge.run",
      args="{\"dem\":\"$dem\",\"coast\":\"$coast\",\"output\":\"$out\",\"seed_x\":114.30,\"seed_y\":30.55,\"tide_level\":36.0,\"frames\":6}"
    );
    mark("stormsurge-ok");
    fit_extent();
    pump(ms=300);
    analysis_set_frame(index=0);
    pump(ms=100);
    analysis_set_frame(index=3);
    pump(ms=100);
    mark("playback-water-tin");
    analysis_export_frames(dir="analysis/stormsurge");
    mark("playback-ok");
    select_map_tab(index=2);
    mark("tab3d");
    pump(ms=200);
    export_bmp(leaf="plugin-showcase-stormsurge-map2d.bmp", frame="stormsurge_coast");
    mark("bmp-ok");
    mark("pass");
    detach_maps();
  }
}
