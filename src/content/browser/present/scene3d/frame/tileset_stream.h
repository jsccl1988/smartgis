// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TILESET_STREAM_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TILESET_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "content/browser/camera/orbit_frame.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/world/world.h"

namespace content {

// Present/orbit seam for 3D Tiles: attach a tileset into World, each pump
// runs select_tiles_limited → apply → ensure_tileset_content (LRU cap).
// WorldPass reads decoded assets via the shared TilesetContentCache.
class TilesetStreamSession {
 public:
  TilesetStreamSession();

  // Parse |json| and attach as kTileset. Replaces any prior attachment.
  bool attach_json(vista::World* world, const char* json, size_t len,
                   const char* name);

  // Optional directory for URI resolve (city_a.glb beside the json). Empty =
  // decode by URI as absolute/path-relative; missing files put_failed.
  void set_content_root(const std::string& root);

  // Per-frame / per-orbit: stream selection + ensure content under budget.
  // Returns true when visible_uris changed.
  bool pump(vista::World* world, const OrbitFrame* orbit, double max_sse = 0,
            size_t max_tiles = 16);

  // Direct view pump (self-test / harness without OrbitFrame).
  bool pump_view(vista::World* world, const vista::ViewState& view,
                 double max_sse = 0, size_t max_tiles = 16);

  bool active() const { return node_id_ != 0; }
  uint64_t node_id() const { return node_id_; }
  vista::TilesetContentCache& cache() { return cache_; }
  const vista::TilesetContentCache& cache() const { return cache_; }
  const std::vector<std::string>& last_visible_uris() const {
    return last_visible_uris_;
  }
  const vista::Tileset& tileset() const { return tileset_; }

  void clear(vista::World* world);

 private:
  static bool resolve_content(const char* uri, vista::ModelAsset* out,
                              size_t* byte_cost, void* user);

  vista::Tileset tileset_;
  vista::TilesetContentCache cache_;
  uint64_t node_id_ = 0;
  std::string content_root_;
  std::string node_name_;
  std::vector<std::string> last_visible_uris_;
};

// Map orbit extent center + distance into a ViewState for select_tiles.
vista::ViewState view_state_from_orbit(const OrbitFrame* orbit);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TILESET_STREAM_H_
