// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/tile_provider.h"

#include <cctype>
#include <cstdio>

#include "base/core/api.h"
#include "gis/tile/wmts.h"

namespace gis {
namespace tile {
namespace {

bool starts_with_ci(const std::string& s, const char* prefix) {
  size_t i = 0;
  for (; prefix[i] && i < s.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(s[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }
  return prefix[i] == '\0';
}

}  // namespace

bool TileProvider::open_xyz(const std::string& url_template) {
  if (url_template.empty()) {
    url_template_.clear();
    return false;
  }
  if (url_template.find("{z}") == std::string::npos ||
      url_template.find("{x}") == std::string::npos ||
      url_template.find("{y}") == std::string::npos) {
    std::fprintf(
        stderr,
        "TileProvider::open_xyz requires {z}/{x}/{y} in URL template\n");
    url_template_.clear();
    return false;
  }
  if (starts_with_ci(url_template, "https://")) {
    // HTTPS needs net httplib built with CPPHTTPLIB_OPENSSL_SUPPORT.
  }
  url_template_ = url_template;
  cache_.clear();
  return true;
}

bool TileProvider::open_wmts_template(const std::string& url_template) {
  std::string normalized;
  if (!normalize_wmts_url_template(url_template, &normalized)) {
    // Already XYZ-shaped?
    return open_xyz(url_template);
  }
  return open_xyz(normalized);
}

bool TileProvider::open_wmts_capabilities(const std::string& capabilities_xml) {
  std::string tmpl;
  std::string err;
  if (!parse_wmts_capabilities(capabilities_xml, &tmpl, &err)) {
    std::fprintf(stderr, "TileProvider::open_wmts_capabilities: %s\n",
                 err.empty() ? "parse failed" : err.c_str());
    url_template_.clear();
    return false;
  }
  return open_wmts_template(tmpl);
}

long TileProvider::guess_image_code(const std::string& url) const {
  // get_image_type_by_file_ext mutates via strlwr; copy first.
  std::string mutable_url = url;
  const long code = get_image_type_by_file_ext(mutable_url.data());
  return code >= 0 ? code : 4;  // PNG default
}

TileImage TileProvider::fetch_tile(const TileCoord& coord, int timeout_sec) {
  TileImage image;
  image.coord = coord;
  image.world_rect = tile_world_rect(coord.z, coord.x, coord.y);
  if (!is_open()) {
    return image;
  }
  if (cache_.try_get(coord, &image)) {
    return image;
  }
  if (disk_.try_get(coord, &image)) {
    cache_.put(image);
    return image;
  }

  const std::string url =
      format_xyz_url(url_template_, coord.z, coord.x, coord.y);
  image.image_code = guess_image_code(url);

  net::HttpResult res;
  if (fetch_fn_) {
    res = fetch_fn_(url);
  } else {
    res = http_.get(url, timeout_sec);
  }
  if (!res.ok || res.body.empty()) {
    std::fprintf(stderr, "TileProvider fetch failed: %s (%s)\n", url.c_str(),
                 res.error.empty() ? "empty body" : res.error.c_str());
    return image;
  }
  image.bytes = std::move(res.body);
  cache_.put(image);
  disk_.put(image);
  return image;
}

std::vector<TileImage> TileProvider::fetch_visible(const Viewport& viewport,
                                                   int timeout_sec) {
  std::vector<TileImage> out;
  const auto coords = tiles_for_viewport(viewport);
  out.reserve(coords.size());
  for (const TileCoord& c : coords) {
    out.push_back(fetch_tile(c, timeout_sec));
  }
  return out;
}

}  // namespace tile
}  // namespace gis
