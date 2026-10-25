// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// URL-template tile fetch with a small in-memory cache. Callers inject a
// transport for tests; the default reads file templates and HTTP(S).

#ifndef GIS_TILE_PROVIDER_XYZ_SOURCE_H_
#define GIS_TILE_PROVIDER_XYZ_SOURCE_H_

#include "gis/gis_export.h"
#include "gis/tile/protocol/lonlat_xyz.h"

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace gis {
namespace tile {

// True and a non-empty body on success. |error| is set on failure.
using TileFetchFn = std::function<bool(const std::string& url, std::string* body,
                                       std::string* error)>;

// One XYZ pyramid. Paint paths should use try_cached and prefetch off the UI thread.
class GIS_EXPORT XyzSource {
 public:
  void set_url_template(std::string url_template);
  const std::string& url_template() const { return url_template_; }

  // Replaces the transport. Empty restores file + HTTP.
  void set_fetch(TileFetchFn fetch);

  bool try_cached(int z, int x, int y, std::string* body) const;

  // Cache hit or one fetch. Network failures leave |error| and return false.
  bool get(int z, int x, int y, std::string* body, std::string* error);

  void clear();

 private:
  std::string url_template_;
  TileFetchFn fetch_;
  mutable std::mutex mu_;
  struct Entry {
    TileIndex index;
    std::string body;
  };
  std::vector<Entry> cache_;
};

// file:// and bare paths, otherwise HTTP(S) via WinINet.
GIS_EXPORT bool fetch_url(const std::string& url, std::string* body,
                          std::string* error, int timeout_sec = 5);

// False when the OS reports no outbound connection. Tests use this to skip.
GIS_EXPORT bool network_available();

}  // namespace tile
}  // namespace gis

#endif  // GIS_TILE_PROVIDER_XYZ_SOURCE_H_
