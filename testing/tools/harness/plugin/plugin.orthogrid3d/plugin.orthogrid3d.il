// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin orthogrid3d.create_hex_grid → MapScene wireframe + .vts.

script "plugin.orthogrid3d" {
  seq @inproc {
    capture_path(leaf="plugin-showcase-orthogrid3d.vts", as="vts");
    require_var(name="vts", fail_mark="vts-path-fail");
    run_processing(
      id="orthogrid3d.create_hex_grid",
      args="{\"nx\":12,\"ny\":12,\"nz\":6,\"vts_path\":\"$vts\",\"corners\":[[0,0,0],[1.2,0,0],[1.35,1.1,0],[0,1,0],[0,0,0.8],[1.15,0.05,0.9],[1.3,1.05,1],[0.05,0.95,0.85]]}"
    );
    mark("orthogrid3d-ok");
    fit_extent();
    pump(ms=300);
    export_bmp(leaf="plugin-showcase-orthogrid3d.bmp", frame="document_extent");
    mark("bmp-ok");
    mark("vts-ok");
    mark("pass");
    detach_maps();
  }
}
