// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_DEM_PRESENT_SURFACE_H_
#define PLUGIN_WORLD3D_DEM_PRESENT_SURFACE_H_

#include "content/public/plugin_host.h"

namespace content {
class GisDocument;
}

namespace plugin {

// DEM tin/grid triangle layer + world3d mesh style.
bool present_world3d_surface(content::GisDocument* doc,
                             content::PluginHost::Scene3dSink* sink,
                             const double* xyz,
                             int point_count,
                             const int* triangles,
                             int triangle_count,
                             const char* op);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_DEM_PRESENT_SURFACE_H_
