// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_ASSETS_TILESET_TILESET_H_
#define VISTA_ASSETS_TILESET_TILESET_H_

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vista/assets/model/model.h"
#include "vista/vista_export.h"

// Explicit 3D Tiles 1.0/1.1 tileset.json. Streaming selection is CPU-side.

namespace vista {

enum class Refine { kReplace, kAdd };

struct Tile {
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  double geometric_error;
  Refine refine;
  std::string content_uri;
  std::vector<Tile> children;

  Tile()
      : min_x(0),
        min_y(0),
        min_z(0),
        max_x(0),
        max_y(0),
        max_z(0),
        geometric_error(0),
        refine(Refine::kReplace) {}
};

struct Tileset {
  Tile root;
};

struct ViewState {
  double eye_x;
  double eye_y;
  double eye_z;
  double sse_denominator;
};

// One resident tile payload. Failed decodes keep a tiny stub so streamers
// do not retry the same URI every frame (AABB fallback stays valid).
struct TilesetContentEntry {
  ModelAsset asset;
  size_t byte_cost = 0;
  bool decode_ok = false;
};

// In-process LRU of decoded tile content capped by |max_bytes|. Used by city
// 3D Tiles streaming so select_tiles + World apply cannot unbounded-grow RAM.
class VISTA_EXPORT TilesetContentCache {
 public:
  explicit TilesetContentCache(size_t max_bytes = 8u * 1024u * 1024u);

  size_t max_bytes() const { return max_bytes_; }
  void set_max_bytes(size_t max_bytes);

  size_t resident_bytes() const { return resident_bytes_; }
  size_t entry_count() const { return map_.size(); }
  void clear();

  // Insert or replace. Charges |byte_cost| (payload size). Evicts LRU until
  // under budget. Returns false when |byte_cost| alone exceeds max_bytes.
  bool put(const std::string& uri, ModelAsset asset, size_t byte_cost,
           bool decode_ok = true);

  // Record a failed resolve so callers skip re-fetch; costs 1 stub byte.
  bool put_failed(const std::string& uri);

  // Touch + lookup. Returns nullptr on miss.
  const TilesetContentEntry* try_get(const std::string& uri);

  bool contains(const std::string& uri) const;

 private:
  using Entry = std::pair<std::string, TilesetContentEntry>;
  using List = std::list<Entry>;

  void evict_overflow();

  size_t max_bytes_;
  size_t resident_bytes_ = 0;
  List list_;
  std::unordered_map<std::string, typename List::iterator> map_;
};

// Loader for ensure_tileset_content. Returns false → put_failed (degrade).
// On success fills |out| and |byte_cost| (must be > 0).
using TilesetContentResolveFn = bool (*)(const char* uri, ModelAsset* out,
                                         size_t* byte_cost, void* user);

VISTA_EXPORT bool parse_tileset_json(const char* json, size_t len, Tileset& out);
VISTA_EXPORT void select_tiles(const Tileset& tileset, const ViewState& view,
                             double max_sse, std::vector<const Tile*>& visible);

// Like select_tiles, then truncates to |max_tiles| (0 = unlimited). Degrades
// gracefully when a view would otherwise select an unbounded leaf set.
VISTA_EXPORT void select_tiles_limited(const Tileset& tileset,
                                     const ViewState& view, double max_sse,
                                     size_t max_tiles,
                                     std::vector<const Tile*>& visible);

// For each selected tile with a non-empty URI: cache hit → touch; miss →
// |resolve| then put / put_failed. Never throws; missing content is OK.
// |max_ensure| caps new resolve attempts this call (0 = unlimited). Warm
// frames with a fully resident selection only touch LRU via try_get.
VISTA_EXPORT void ensure_tileset_content(
    const std::vector<const Tile*>& visible, TilesetContentCache* cache,
    TilesetContentResolveFn resolve, void* user, size_t max_ensure = 0);

}  // namespace vista

#endif  // VISTA_ASSETS_TILESET_TILESET_H_
