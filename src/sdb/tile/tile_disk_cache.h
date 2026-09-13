// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_TILE_DISK_CACHE_H_
#define SDB_TILE_TILE_DISK_CACHE_H_

#include "sdb/tile/tile_cache.h"

#include "sdb/gis_export.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace sdb {
namespace tile {

// Optional on-disk tile store keyed by z/x/y. Bounded by entry count.
// Empty directory disables the cache. Not shared across processes safely.
class GIS_EXPORT TileDiskCache {
 public:
  void set_directory(std::string dir) { directory_ = std::move(dir); }
  const std::string& directory() const { return directory_; }
  bool is_enabled() const { return !directory_.empty(); }

  void set_capacity(size_t capacity) {
    capacity_ = capacity ? capacity : 1;
  }
  size_t capacity() const { return capacity_; }

  // Loads bytes + metadata into |out| (coord must already be set by caller
  // or is overwritten from the key). Returns false on miss / I/O error.
  bool try_get(const TileCoord& coord, TileImage* out) const;

  // Writes a successful fetch. Ignores empty bodies. Evicts oldest files
  // when over capacity (by directory entry count, best-effort).
  void put(const TileImage& image) const;

  void clear() const;

 private:
  std::filesystem::path file_path(const TileCoord& coord) const;
  void evict_overflow() const;

  std::string directory_;
  size_t capacity_ = 1024;
};

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_TILE_DISK_CACHE_H_
