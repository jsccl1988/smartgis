// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/ingest/seed_paths.h"

#include "content/public/map_bootstrap.h"

namespace content {
namespace detail {

std::string path_stem(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  size_t begin = path.find_last_of("/\\");
  begin = (begin == std::string::npos) ? 0 : begin + 1;
  size_t end = path.find_last_of('.');
  if (end == std::string::npos || end < begin) {
    end = path.size();
  }
  return path.substr(begin, end - begin);
}

std::vector<std::string> style_seed_relative_paths() {
  // Intentionally empty: china_city.style.json disables carto remap. Hosts
  // that need an on-disk style must pass an explicit path; default china seed
  // uses in-memory default_carto_style_json via a null StyleDocument.
  return {};
}

}  // namespace detail

std::vector<std::string> china_seed_relative_paths() {
  // Delegate to map_bootstrap SoT so bare launch / harness / seed_default
  // never diverge on candidate order (china_city before china_plp).
  return sample_map_relative_paths();
}

bool try_resolve_china_seed_path(const std::string& exe_dir,
                                 std::string* out_path) {
  return try_resolve_existing_sample_map({exe_dir}, out_path);
}

}  // namespace content
