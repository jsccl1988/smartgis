// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// NOTE: suite plugin.world3d gates on Scene3D C++ marks from
// plugin_showcase.cc (pointcloud-ok / present-ok / bmp-ok / pass). This .il
// Map2d export path is CapabilityHost regression only — not the loop suite
// body, and its BMP is not the product Scene3D screenshot.

script "plugin.world3d" {
  seq @inproc {
    resolve_data(kind="plugin", leaf="world3d_tin_sample.xyz", as="tin");
    require_var(name="tin", fail_mark="tin-missing");
    resolve_data(kind="plugin", leaf="world3d_dem_sample.tif", as="dem");
    require_var(name="dem", fail_mark="dem-missing");
    resolve_data(kind="data", leaf="pointcloud_public_sample.txt", as="cloud", soft=1);
    resolve_data(kind="plugin", leaf="world3d_pointcloud_sample.las", as="cloud", soft=1);
    require_var(name="cloud", fail_mark="pointcloud-missing");
    doc_clear();
    run_processing(id="world3d.tin_from_xyz",
                   args="{\"vertex_path\":\"$tin\",\"separator\":\"comma\",\"head_skip\":1,\"line_skip\":0,\"col_x\":0,\"col_y\":1,\"col_z\":2,\"x_scale\":1,\"y_scale\":1,\"z_scale\":1}");
    mark("tin-ok");
    run_processing(id="world3d.grid_from_heightmap",
                   args="{\"heightmap_path\":\"$dem\",\"x_scale\":1,\"y_scale\":1,\"z_scale\":0.001}");
    mark("grid-ok");
    run_processing(id="model3d.add_pointcloud", args="{\"path\":\"$cloud\"}");
    mark("pointcloud-ok");
    run_processing(id="model3d.add_sphere", args="{}");
    mark("sphere-ok");
    resolve_data(kind="harness", leaf="world3d_mesh.style.json", as="style");
    require_var(name="style", fail_mark="style-missing");
    apply_style_file(var="style");
    mark("style-ok");
    invalidate_map2d();
    pump(ms=300);
    export_bmp(leaf="plugin-showcase-world3d-map2d.bmp", frame="world3d_tin");
    mark("bmp-ok");
    mark("pass");
    detach_maps();
  }
}
