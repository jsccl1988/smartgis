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
#include "vista/component/world/world.h"

namespace content {

// Present/orbit seam for 3D Tiles: attach a tileset into World, each pump
// runs select_tiles_limited → apply → ensure_tileset_content (LRU cap).
// WorldPass reads decoded assets via the shared TilesetContentCache.
// Product SoT stays Vista WorldPass; this session only feeds the cache.
class TilesetStreamSession {
 public:
  // Content present defaults (wired by Scene3dGpuPresent product pump).
  // max_ensure=2 keeps decode spikes off the warm frame; leftover misses
  // drain across subsequent pumps. Cache matches vista default (8 MiB LRU).
  static constexpr double kDefaultMaxSse = 0;
  static constexpr size_t kDefaultMaxTiles = 16;
  static constexpr size_t kDefaultMaxEnsure = 2;
  static constexpr size_t kDefaultCacheBytes = 8u * 1024u * 1024u;

  TilesetStreamSession();

  // Parse |json| and attach as kTileset. Replaces any prior attachment.
  bool attach_json(vista::World* world, const char* json, size_t len,
                   const char* name);

  // Optional directory for URI resolve (city_a.glb beside the json). Empty =
  // decode by URI as absolute/path-relative; missing files put_failed.
  void set_content_root(const std::string& root);

  // Optional LRU budget override (bytes). Eviction is vista TilesetContentCache.
  void set_cache_max_bytes(size_t max_bytes);

  // Per-frame / per-orbit: stream selection + ensure content under budget.
  // Returns true when visible_uris changed.
  // |max_tiles| caps select; |max_ensure| caps new decode resolves per pump
  // (0 = ensure every selected miss, up to |max_tiles|).
  bool pump(vista::World* world, const OrbitFrame* orbit,
            double max_sse = kDefaultMaxSse,
            size_t max_tiles = kDefaultMaxTiles,
            size_t max_ensure = kDefaultMaxEnsure);

  // Direct view pump (self-test / harness without OrbitFrame).
  bool pump_view(vista::World* world, const vista::ViewState& view,
                 double max_sse = kDefaultMaxSse,
                 size_t max_tiles = kDefaultMaxTiles,
                 size_t max_ensure = kDefaultMaxEnsure);

  // Product present shorthand: default SSE / tile / ensure budgets.
  bool pump_product(vista::World* world, const OrbitFrame* orbit) {
    return pump(world, orbit, kDefaultMaxSse, kDefaultMaxTiles,
                kDefaultMaxEnsure);
  }

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
