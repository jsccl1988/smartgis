// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_TILE_PROVIDER_H_
#define SDB_TILE_TILE_PROVIDER_H_

#include "net/http/http.h"
#include "sdb/gis_export.h"
#include "sdb/tile/tile_cache.h"
#include "sdb/tile/tile_disk_cache.h"
#include "sdb/tile/xyz_math.h"

#include <functional>
#include <string>
#include <vector>

namespace sdb {
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

  // Optional disk cache. Empty |dir| disables. Capacity is file count.
  void set_disk_cache_dir(const std::string& dir) {
    disk_.set_directory(dir);
  }
  void set_disk_cache_capacity(size_t capacity) { disk_.set_capacity(capacity); }
  const std::string& disk_cache_dir() const { return disk_.directory(); }
  void clear_disk_cache() { disk_.clear(); }

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
};

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_TILE_PROVIDER_H_
