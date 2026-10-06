// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_MAP2D_SEED_ORTHOGRID_MESH_H_
#define APP_VIEWS_HARNESS_SHOWCASE_MAP2D_SEED_ORTHOGRID_MESH_H_

namespace app {

class Browser;

namespace detail {

// Coastal bay gridbnd (nx=ny=33) 鈫?Laplace + Thompson 鈫?heat mesh in MapScene.
bool load_map2d_orthogrid_mesh(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_MAP2D_SEED_ORTHOGRID_MESH_H_
