// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_SCENE_TESSELLATE_H_
#define SDB_SCENE_TESSELLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

// CPU tessellation of leftover SmtGeometry / Smt3DGeometry into GPU-uploadable
// xyz + indices. No render/rhi types.

namespace Smt_Geo {
class SmtGeometry;
}

class OGRLayer;

namespace Smt_3DGeo {
class Smt3DGeometry;
}

namespace sdb {
namespace scene {

// Triangle mesh produced from leftover GIS geometry (2D features or 3D surface).
struct TessMesh {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
};

bool tessellate_geometry(const Smt_Geo::SmtGeometry* geom, TessMesh& out);
bool tessellate_geoms(const Smt_Geo::SmtGeometry* const* geoms, size_t count,
                      TessMesh& out);
bool tessellate_layer(OGRLayer* layer, TessMesh& out);
bool tessellate_3d_geometry(const Smt_3DGeo::Smt3DGeometry* geom, TessMesh& out);

}  // namespace scene
}  // namespace sdb

#endif  // SDB_SCENE_TESSELLATE_H_
