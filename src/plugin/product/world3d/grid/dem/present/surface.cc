// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/dem/present/surface.h"

#include "content/public/gis_document.h"
#include "plugin/product/world3d/scene/present/style.h"

#include <cstring>

namespace plugin {

bool present_world3d_surface(content::GisDocument* doc,
                             content::PluginHost::Scene3dSink* sink,
                             const double* xyz,
                             int point_count,
                             const int* triangles,
                             int triangle_count,
                             const char* op) {
  if (!doc || !xyz || !triangles || point_count < 3 || triangle_count < 1) {
    return false;
  }
  const char* name = (op && std::strstr(op, "grid")) ? "DEM grid" : "DEM tin";
  if (!doc->add_triangle_mesh(name, xyz, point_count, triangles,
                              triangle_count)) {
    return false;
  }
  (void)apply_world3d_mesh_style(doc);
  if (sink) {
    sink->invalidate();
  }
  return true;
}

}  // namespace plugin
