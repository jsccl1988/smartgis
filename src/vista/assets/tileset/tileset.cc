// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/assets/tileset/tileset.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "base/memory/arena.h"
#include "base/memory/object_pool.h"

namespace vista {
namespace {

// Recycle temporary parse key buffers across tileset JSON parses.
base::ObjectPool<std::string>& tileset_key_pool() {
  static base::ObjectPool<std::string> pool(
      64, nullptr, [](std::string* s) { s->clear(); });
  return pool;
}

struct Parser {
  const char* cur;
  const char* end;
  base::MemoryResource* scratch = nullptr;

  void skip() {
    while (cur < end) {
      const char c = *cur;
      if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        break;
      }
      ++cur;
    }
  }

  bool eat(char c) {
    skip();
    if (cur < end && *cur == c) {
      ++cur;
      return true;
    }
    return false;
  }

  bool parse_string(std::string& out) {
    skip();
    if (!eat('"')) {
      return false;
    }
    out.clear();
    while (cur < end && *cur != '"') {
      if (*cur == '\\' && cur + 1 < end) {
        ++cur;
        out.push_back(*cur++);
      } else {
        out.push_back(*cur++);
      }
    }
    return eat('"');
  }

  bool parse_number(double& out) {
    skip();
    if (cur >= end) {
      return false;
    }
    char* stop = nullptr;
    out = std::strtod(cur, &stop);
    if (stop == cur) {
      return false;
    }
    cur = stop;
    return true;
  }

  bool skip_value() {
    skip();
    if (cur >= end) {
      return false;
    }
    if (*cur == '"') {
      auto tmp = tileset_key_pool().allocate();
      return parse_string(*tmp);
    }
    if (*cur == '{') {
      return skip_object();
    }
    if (*cur == '[') {
      return skip_array();
    }
    if (std::strncmp(cur, "true", 4) == 0) {
      cur += 4;
      return true;
    }
    if (std::strncmp(cur, "false", 5) == 0) {
      cur += 5;
      return true;
    }
    if (std::strncmp(cur, "null", 4) == 0) {
      cur += 4;
      return true;
    }
    double n;
    return parse_number(n);
  }

  bool skip_object() {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':') || !skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool skip_array() {
    if (!eat('[')) {
      return false;
    }
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      if (!skip_value()) {
        return false;
      }
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_box_aabb(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    double v[12];
    for (int i = 0; i < 12; ++i) {
      if (!parse_number(v[i])) {
        return false;
      }
      skip();
      if (i < 11 && !eat(',')) {
        return false;
      }
    }
    if (!eat(']')) {
      return false;
    }
    const double cx = v[0];
    const double cy = v[1];
    const double cz = v[2];
    const double hx = std::fabs(v[3]) + std::fabs(v[6]) + std::fabs(v[9]);
    const double hy = std::fabs(v[4]) + std::fabs(v[7]) + std::fabs(v[10]);
    const double hz = std::fabs(v[5]) + std::fabs(v[8]) + std::fabs(v[11]);
    tile.min_x = cx - hx;
    tile.max_x = cx + hx;
    tile.min_y = cy - hy;
    tile.max_y = cy + hy;
    tile.min_z = cz - hz;
    tile.max_z = cz + hz;
    return true;
  }

  bool parse_region_aabb(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    double v[6];
    for (int i = 0; i < 6; ++i) {
      if (!parse_number(v[i])) {
        return false;
      }
      skip();
      if (i < 5 && !eat(',')) {
        return false;
      }
    }
    if (!eat(']')) {
      return false;
    }
    tile.min_x = v[0];
    tile.min_y = v[1];
    tile.min_z = v[4];
    tile.max_x = v[2];
    tile.max_y = v[3];
    tile.max_z = v[5];
    return true;
  }

  bool parse_bounding_volume(Tile& tile) {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    bool ok_vol = false;
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "box") {
        ok_vol = parse_box_aabb(tile);
      } else if (key == "region") {
        ok_vol = parse_region_aabb(tile);
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return ok_vol;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_content(Tile& tile) {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "uri") {
        if (!parse_string(tile.content_uri)) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_children(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      Tile child;
      if (!parse_tile(child)) {
        return false;
      }
      tile.children.push_back(std::move(child));
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_tile(Tile& tile) {
    tile = Tile();
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    bool saw_volume = false;
    for (;;) {
      auto key_holder = tileset_key_pool().allocate();
      std::string& key = *key_holder;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "boundingVolume") {
        saw_volume = parse_bounding_volume(tile);
        if (!saw_volume) {
          return false;
        }
      } else if (key == "geometricError") {
        if (!parse_number(tile.geometric_error)) {
          return false;
        }
      } else if (key == "refine") {
        std::string refine;
        if (!parse_string(refine)) {
          return false;
        }
        tile.refine = (refine == "ADD") ? Refine::kAdd : Refine::kReplace;
      } else if (key == "content") {
        if (!parse_content(tile)) {
          return false;
        }
      } else if (key == "children") {
        if (!parse_children(tile)) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return saw_volume;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_tileset(Tileset& out) {
    skip();
    if (!eat('{')) {
      return false;
    }
    skip();
    bool have_root = false;
    if (eat('}')) {
      return false;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "root") {
        have_root = parse_tile(out.root);
        if (!have_root) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return have_root;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }
};

double aabb_center_distance(const Tile& tile, const ViewState& view) {
  const double cx = 0.5 * (tile.min_x + tile.max_x);
  const double cy = 0.5 * (tile.min_y + tile.max_y);
  const double cz = 0.5 * (tile.min_z + tile.max_z);
  const double dx = cx - view.eye_x;
  const double dy = cy - view.eye_y;
  const double dz = cz - view.eye_z;
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  return d < 1e-6 ? 1e-6 : d;
}

void select_walk(const Tile& tile, const ViewState& view, double max_sse,
                 std::vector<const Tile*>& visible) {
  const double sse = tile.geometric_error * view.sse_denominator /
                     aabb_center_distance(tile, view);
  const bool refine = sse > max_sse && !tile.children.empty();
  if (!refine) {
    visible.push_back(&tile);
    return;
  }
  if (tile.refine == Refine::kAdd) {
    visible.push_back(&tile);
  }
  for (const Tile& child : tile.children) {
    select_walk(child, view, max_sse, visible);
  }
}

}  // namespace

bool parse_tileset_json(const char* json, size_t len, Tileset& out) {
  out = Tileset();
  if (!json || len == 0) {
    return false;
  }
  base::Arena scratch(base::MemoryResource::Type::kMonotonicBuffer, 256 * 1024);
  Parser p;
  p.cur = json;
  p.end = json + len;
  p.scratch = scratch.memory_resource.get();
  // Keep bump allocation so the monotonic arena is live for nested temps.
  if (p.scratch) {
    (void)p.scratch->allocate(64, 8);
  }
  return p.parse_tileset(out);
}

void select_tiles(const Tileset& tileset, const ViewState& view, double max_sse,
                  std::vector<const Tile*>& visible) {
  visible.clear();
  select_walk(tileset.root, view, max_sse, visible);
}

void select_tiles_limited(const Tileset& tileset, const ViewState& view,
                          double max_sse, size_t max_tiles,
                          std::vector<const Tile*>& visible) {
  select_tiles(tileset, view, max_sse, visible);
  if (max_tiles > 0 && visible.size() > max_tiles) {
    visible.resize(max_tiles);
  }
}

TilesetContentCache::TilesetContentCache(size_t max_bytes)
    : max_bytes_(max_bytes ? max_bytes : 1) {}

void TilesetContentCache::set_max_bytes(size_t max_bytes) {
  max_bytes_ = max_bytes ? max_bytes : 1;
  evict_overflow();
}

void TilesetContentCache::clear() {
  list_.clear();
  map_.clear();
  resident_bytes_ = 0;
}

bool TilesetContentCache::put(const std::string& uri, ModelAsset asset,
                              size_t byte_cost, bool decode_ok) {
  if (uri.empty() || byte_cost == 0 || byte_cost > max_bytes_) {
    return false;
  }
  auto it = map_.find(uri);
  if (it != map_.end()) {
    resident_bytes_ -= it->second->second.byte_cost;
    it->second->second.asset = std::move(asset);
    it->second->second.byte_cost = byte_cost;
    it->second->second.decode_ok = decode_ok;
    resident_bytes_ += byte_cost;
    list_.splice(list_.begin(), list_, it->second);
    evict_overflow();
    return true;
  }
  TilesetContentEntry entry;
  entry.asset = std::move(asset);
  entry.byte_cost = byte_cost;
  entry.decode_ok = decode_ok;
  list_.emplace_front(uri, std::move(entry));
  map_[list_.front().first] = list_.begin();
  resident_bytes_ += byte_cost;
  evict_overflow();
  return true;
}

bool TilesetContentCache::put_failed(const std::string& uri) {
  ModelAsset empty;
  return put(uri, std::move(empty), 1, false);
}

const TilesetContentEntry* TilesetContentCache::try_get(const std::string& uri) {
  auto it = map_.find(uri);
  if (it == map_.end()) {
    return nullptr;
  }
  list_.splice(list_.begin(), list_, it->second);
  return &it->second->second;
}

bool TilesetContentCache::contains(const std::string& uri) const {
  return map_.find(uri) != map_.end();
}

void TilesetContentCache::evict_overflow() {
  while (!list_.empty() && resident_bytes_ > max_bytes_) {
    const TilesetContentEntry& back = list_.back().second;
    resident_bytes_ -= back.byte_cost;
    map_.erase(list_.back().first);
    list_.pop_back();
  }
}

void ensure_tileset_content(const std::vector<const Tile*>& visible,
                            TilesetContentCache* cache,
                            TilesetContentResolveFn resolve, void* user,
                            size_t max_ensure) {
  if (!cache) {
    return;
  }
  size_t ensured = 0;
  for (const Tile* tile : visible) {
    if (!tile || tile->content_uri.empty()) {
      continue;
    }
    if (cache->try_get(tile->content_uri) != nullptr) {
      continue;
    }
    if (max_ensure > 0 && ensured >= max_ensure) {
      break;
    }
    if (!resolve) {
      cache->put_failed(tile->content_uri);
      ++ensured;
      continue;
    }
    ModelAsset asset;
    size_t cost = 0;
    if (resolve(tile->content_uri.c_str(), &asset, &cost, user) && cost > 0) {
      if (!cache->put(tile->content_uri, std::move(asset), cost, true)) {
        cache->put_failed(tile->content_uri);
      }
    } else {
      cache->put_failed(tile->content_uri);
    }
    ++ensured;
  }
}

}  // namespace vista
