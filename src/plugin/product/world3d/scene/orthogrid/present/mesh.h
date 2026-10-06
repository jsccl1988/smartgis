// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_ORTHOGRID_PRESENT_MESH_H_
#define PLUGIN_WORLD3D_ORTHOGRID_PRESENT_MESH_H_

namespace content {
class GisDocument;
}

namespace plugin {
struct OrthogridMeshCommit;

// Map2d mesh lines + orthogonality heat rasters (face 0).
bool present_orthogrid_mesh(content::GisDocument* doc,
                            const OrthogridMeshCommit& mesh);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_ORTHOGRID_PRESENT_MESH_H_
