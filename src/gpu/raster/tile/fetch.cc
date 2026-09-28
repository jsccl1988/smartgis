// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/frame_sink.h"

#include "net/http/http.h"

#include <string>

namespace gpu {

// Tile TileFetchFn injected into DrawRequest. Not a second HTTP cache:
// each call is one net::HttpClient::get, same stack as TileProvider.
TileFetchFn make_net_tile_fetch() {
  return [](const std::string& url) -> TileFetchResult {
    TileFetchResult out;
    if (url.empty()) {
      return out;
    }
    try {
      net::HttpClient client;
      const net::HttpResult hr = client.get(url, /*timeout_sec=*/5);
      if (!hr.ok || hr.body.empty()) {
        return out;
      }
      out.ok = true;
      out.body = hr.body;
    } catch (...) {
      // Paint path must not throw; caller treats ok=false as skip-raster.
    }
    return out;
  };
}

}  // namespace gpu
