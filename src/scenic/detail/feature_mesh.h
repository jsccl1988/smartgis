// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_FEATURE_MESH_H_
#define SCENIC_DETAIL_FEATURE_MESH_H_

#include <cstdint>
#include <vector>

#include "scenic/scenic_impl_export.h"

class OGRGeometry;
class OGRTriangulatedSurface;

namespace scenic {
namespace detail {

// CPU vertex for scenic scene3d feature drawables.
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

// Device-free tessellation result uploaded by scene3d GeoObject.
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
LEGACY_RENDER_EXPORT bool tess_map_geometry(const OGRGeometry& geom,
                                            const FeatureRgb& stroke,
                                            const FeatureRgb& fill,
                                            FeatureHeightSampleFn height_fn,
                                            void* height_user,
                                            FeatureMesh* out);

// World-frame OGR (X,Y,Z) → VB (X, Z, Y).
LEGACY_RENDER_EXPORT bool tess_world_geometry(const OGRGeometry& geom,
                                              FeatureMesh* out);

LEGACY_RENDER_EXPORT bool tess_3d_surface(const OGRTriangulatedSurface& surf,
                                          FeatureMesh* out);

}  // namespace detail
}  // namespace scenic

// Historical `namespace render` aliases used by scene3d TUs.
namespace render {
using FeatureVertex = ::scenic::detail::FeatureVertex;
using FeaturePrim = ::scenic::detail::FeaturePrim;
using FeatureMesh = ::scenic::detail::FeatureMesh;
using FeatureHeightSampleFn = ::scenic::detail::FeatureHeightSampleFn;
using FeatureRgb = ::scenic::detail::FeatureRgb;
using ::scenic::detail::tess_map_geometry;
using ::scenic::detail::tess_world_geometry;
using ::scenic::detail::tess_3d_surface;
}  // namespace render

#endif  // SCENIC_DETAIL_FEATURE_MESH_H_
