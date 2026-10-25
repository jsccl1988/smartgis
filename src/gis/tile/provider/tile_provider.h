// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_TILE_TILE_PROVIDER_H_
#define GIS_TILE_TILE_PROVIDER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "net/http/http.h"
#include "gis/gis_export.h"
#include "gis/tile/cache/tile_cache.h"
#include "gis/tile/cache/tile_disk_cache.h"
#include "gis/tile/protocol/xyz_math.h"

namespace gis {
namespace tile {

// Independent HTTP(S) XYZ / WMTS-template tile source. Not OGR / SDBD:MEM.
// HTTPS requires //src/net with CPPHTTPLIB_OPENSSL_SUPPORT.
class GIS_EXPORT TileProvider {
 public:
  using FetchFn = std::function<net::HttpResult(const std::string& url)>;

  bool open_xyz(const std::string& url_template);
  // Accepts XYZ or WMTS placeholders ({TileMatrix}/{TileCol}/{TileRow}).
  bool open_wmts_template(const std::string& url_template);
  // Parse Capabilities XML then open_wmts_template.
  bool open_wmts_capabilities(const std::string& capabilities_xml);

  const std::string& url_template() const { return url_template_; }
  bool is_open() const { return !url_template_.empty(); }

  // Optional inject for offline tests; unset → net::HttpClient::get.
  void set_fetch_fn(FetchFn fn) { fetch_fn_ = std::move(fn); }

  // Process-local LRU (default 256).
  void set_cache_capacity(size_t capacity) { cache_.set_capacity(capacity); }
  size_t cache_size() const { return cache_.size(); }
  void clear_cache() { cache_.clear(); }

  // Disk cache (enabled by default on open_xyz for production paths).
  // Empty |dir| disables. Capacity is file count.
  void set_disk_cache_dir(const std::string& dir) { disk_.set_directory(dir); }
  void set_disk_cache_capacity(size_t capacity) {
    disk_.set_capacity(capacity);
  }
  const std::string& disk_cache_dir() const { return disk_.directory(); }
  void clear_disk_cache() { disk_.clear(); }
  bool disk_cache_enabled() const { return disk_.is_enabled(); }
  // Production MapLayer / registry paths call this after open_xyz.
  void enable_production_disk_cache();

  // Counters since open (memory hits are not counted).
  uint64_t disk_cache_hits() const { return disk_cache_hits_; }
  uint64_t http_tile_fetches() const { return http_tile_fetches_; }

  TileImage fetch_tile(const TileCoord& coord, int timeout_sec = 5);
  std::vector<TileImage> fetch_visible(const Viewport& viewport,
                                       int timeout_sec = 5);

 private:
  long guess_image_code(const std::string& url) const;

  std::string url_template_;
  FetchFn fetch_fn_;
  net::HttpClient http_;
  TileCache cache_;
  TileDiskCache disk_;
  uint64_t disk_cache_hits_ = 0;
  uint64_t http_tile_fetches_ = 0;
};

}  // namespace tile
}  // namespace gis

#endif  // GIS_TILE_TILE_PROVIDER_H_
