// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_FEATURE_MESH_H_
#define SMT_LEGACY_GIS_FEATURE_MESH_H_

#include <cstdint>
#include <vector>

#include "gis/gis_export.h"

class OGRGeometry;
class OGRTriangulatedSurface;

namespace render {

// CPU vertex for leftover feature drawables. No RHI / SmtScene types.
struct FeatureVertex {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float nx = 0.f;
  float ny = 1.f;
  float nz = 0.f;
  float r = 1.f;
  float g = 1.f;
  float b = 1.f;
  float a = 1.f;
  bool has_normal = false;
  bool has_color = false;
};

enum class FeaturePrim : std::uint8_t {
  kPoints = 0,
  kLineStrip = 1,
  kTriangles = 2,
  kLineList = 3,
};

// Device-free tessellation result uploaded by scene3d SmtGeoObject.
struct FeatureMesh {
  std::vector<FeatureVertex> vertices;
  std::vector<std::uint32_t> indices;
  FeaturePrim prim = FeaturePrim::kPoints;
  bool indexed = false;
  bool has_normals = false;
  bool has_colors = false;
};

using FeatureHeightSampleFn = float (*)(double x, double y, void* user);

struct FeatureRgb {
  float r = 1.f;
  float g = 1.f;
  float b = 1.f;
};

// Map-frame OGR (lon/lat) → mesh. Optional height sample drapes onto DEM.
// Vertex X is -lon for east-on-right leftover framing.
GIS_EXPORT bool tess_map_geometry(const OGRGeometry& geom,
                                  const FeatureRgb& stroke,
                                  const FeatureRgb& fill,
                                  FeatureHeightSampleFn height_fn,
                                  void* height_user, FeatureMesh* out);

// True leftover 3D coordinates: VB gets (X, Z, Y) from OGR (X, Y, Z).
GIS_EXPORT bool tess_world_geometry(const OGRGeometry& geom, FeatureMesh* out);

GIS_EXPORT bool tess_3d_surface(const OGRTriangulatedSurface& surf, FeatureMesh* out);

}  // namespace render

#endif  // SMT_LEGACY_GIS_FEATURE_MESH_H_
