// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// URL-template tile fetch with a small in-memory cache. Callers inject a
// transport for tests; the default reads file templates and HTTP(S).

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_XYZ_SOURCE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_XYZ_SOURCE_H_

#include "content/browser/glang.runtime/host/tile/web_mercator.h"

#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace content {
namespace tile {

// True and a non-empty body on success. |error| is set on failure.
using TileFetchFn = std::function<bool(const std::string& url, std::string* body,
                                       std::string* error)>;

// One XYZ pyramid. Paint paths should use try_cached and prefetch off the UI thread.
class XyzSource {
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
bool fetch_url(const std::string& url, std::string* body, std::string* error,
               int timeout_sec = 5);

// False when the OS reports no outbound connection. Tests use this to skip.
bool network_available();

}  // namespace tile
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_XYZ_SOURCE_H_
