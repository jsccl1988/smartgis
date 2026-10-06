// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEXGRID_PRESENT_MESH_H_
#define PLUGIN_WORLD3D_HEXGRID_PRESENT_MESH_H_

namespace content {
class GisDocument;
class Scene3dPresenter;
}

namespace plugin {
class Scene3dSink;
struct HexGridCommit;

// Map2d hex lab pad + optional Scene3D closed volume overlay.
bool present_hex_grid_mesh(content::GisDocument* doc,
                           Scene3dSink* sink,
                           content::Scene3dPresenter* scene3d,
                           const HexGridCommit& commit);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEXGRID_PRESENT_MESH_H_
