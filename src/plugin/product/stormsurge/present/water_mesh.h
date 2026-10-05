// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_
#define PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_

#include "content/public/plugin_host.h"

namespace content {
class GisDocument;
class Scene3dPresenter;
}

namespace plugin {

// Map triangle mesh + optional Scene3D free-surface overlay (chrome presenter).
bool present_stormsurge_water_mesh(content::GisDocument* doc,
                                   content::PluginHost::Scene3dSink* sink,
                                   content::Scene3dPresenter* scene3d,
                                   const double* xyz,
                                   int point_count,
                                   const int* triangles,
                                   int triangle_count);

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_PRESENT_WATER_MESH_H_
