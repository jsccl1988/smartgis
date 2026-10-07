// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_EARTH_TILES_H_
#define PLUGIN_WORLD3D_SCENE_EARTH_TILES_H_

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace plugin {

// Result of best-effort M3 city tileset attach under exe-relative samples.
enum class World3dCityTilesResult {
  kAttached,      // JSON attached (or attach returned true)
  kAttachFailed,  // fixture found but attach_tileset_json failed
  kSkipped,       // no city_root.glb / tileset under exe
};

// Clears any prior tileset, then tries m3_city_tileset.json next to
// city_root.glb under the executable directory.
World3dCityTilesResult try_attach_world3d_city_tiles(
    content::Scene3dPresenter* cam);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_EARTH_TILES_H_
