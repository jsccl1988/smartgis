// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_
#define PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_

namespace content {
class GisDocument;
class Scene3dPresenter;
}

namespace plugin {
class Scene3dSink;

// Map triangle mesh + optional Scene3D free-surface overlay (horizon presenter).
// When |depth| + |geotransform| match the DEM grid, the overlay drape bakes a
// surface grid and depth isolines (vista contour paint) onto the water atlas.
bool present_stormsurge_water_mesh(content::GisDocument* doc,
                                   Scene3dSink* sink,
                                   content::Scene3dPresenter* scene3d,
                                   const double* xyz,
                                   int point_count,
                                   const int* triangles,
                                   int triangle_count,
                                   const float* depth = nullptr,
                                   int dem_width = 0,
                                   int dem_height = 0,
                                   const double* geotransform = nullptr);

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_
