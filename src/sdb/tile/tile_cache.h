// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_TILE_CACHE_H_
#define SDB_TILE_TILE_CACHE_H_

#include "sdb/tile/xyz_math.h"

#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

namespace sdb {
namespace tile {

// One fetched XYZ tile: encoded image bytes + Web Mercator world rect.
struct TileImage {
  TileCoord coord;
  std::string bytes;
  base::fRect world_rect{};
  long image_code = 4;  // CXIMAGE_FORMAT_PNG
};

inline bool operator==(const TileCoord& a, const TileCoord& b) {
  return a.z == b.z && a.x == b.x && a.y == b.y;
}

// In-process LRU of successful tile fetches. Disk cache is Phase 2+ later.
class TileCache {
 public:
  explicit TileCache(size_t capacity = 256) : capacity_(capacity ? capacity : 1) {}

  size_t capacity() const { return capacity_; }
  void set_capacity(size_t capacity) {
    capacity_ = capacity ? capacity : 1;
    evict_overflow();
  }

  size_t size() const { return map_.size(); }
  void clear() {
    list_.clear();
    map_.clear();
  }

  // Copies a cached hit into |out|. Returns false on miss.
  bool try_get(const TileCoord& coord, TileImage* out) {
    if (out == nullptr) {
      return false;
    }
    auto it = map_.find(coord);
    if (it == map_.end()) {
      return false;
    }
    list_.splice(list_.begin(), list_, it->second);
    *out = it->second->second;
    return true;
  }

  // Stores a successful fetch. Empty bodies are ignored.
  void put(TileImage image) {
    if (image.bytes.empty()) {
      return;
    }
    auto it = map_.find(image.coord);
    if (it != map_.end()) {
      it->second->second = std::move(image);
      list_.splice(list_.begin(), list_, it->second);
      return;
    }
    list_.emplace_front(image.coord, std::move(image));
    map_[list_.front().first] = list_.begin();
    evict_overflow();
  }

 private:
  struct CoordHash {
    size_t operator()(const TileCoord& c) const noexcept {
      const auto z = static_cast<uint64_t>(static_cast<uint32_t>(c.z));
      const auto x = static_cast<uint64_t>(static_cast<uint32_t>(c.x));
      const auto y = static_cast<uint64_t>(static_cast<uint32_t>(c.y));
      return static_cast<size_t>((z * 1315423911ull) ^ (x << 1) ^ (y << 2));
    }
  };

  using Entry = std::pair<TileCoord, TileImage>;
  using List = std::list<Entry>;

  void evict_overflow() {
    while (map_.size() > capacity_) {
      const TileCoord& key = list_.back().first;
      map_.erase(key);
      list_.pop_back();
    }
  }

  size_t capacity_;
  List list_;
  std::unordered_map<TileCoord, typename List::iterator, CoordHash> map_;
};

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_TILE_CACHE_H_
