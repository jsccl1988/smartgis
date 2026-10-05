// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/map_bootstrap.h"

#include <filesystem>
#include <string>
#include <vector>

namespace content {
namespace {

// Product China pack policy (locked): bare SmartGIS.exe, MapScene::seed_default,
// and harness openers must resolve the same file. GN china_map_samples writes
// to out/data (exe is out/Debug|Release → ../data). Real packs only — no
// views_ogr_sample / synthetic stub. Every china_city candidate precedes any
// china_plp fallback so a schematic PLP under out/data cannot shadow a
// prefecture pack under testing/data.
const char* k_relative_candidates[] = {
    "../data/china_city.gpkg",
    "../data/china_city.geojson",
    "data/china_city.gpkg",
    "data/china_city.geojson",
    "china_city.gpkg",
    "china_city.geojson",
    "testing/data/china/china_city.gpkg",
    "testing/data/china/china_city.geojson",
    "../testing/data/china/china_city.gpkg",
    "../testing/data/china/china_city.geojson",
    "../../testing/data/china/china_city.gpkg",
    "../../testing/data/china/china_city.geojson",
    "../../../testing/data/china/china_city.gpkg",
    "../../../testing/data/china/china_city.geojson",
    // Schematic PLP fallback (only after every china_city candidate).
    "../data/china_plp.geojson",
    "data/china_plp.geojson",
    "china_plp.geojson",
    "testing/data/china/china_plp.geojson",
    "../testing/data/china/china_plp.geojson",
    "../../testing/data/china/china_plp.geojson",
    "../../../testing/data/china/china_plp.geojson",
};

bool path_is_regular_file(const std::string& path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(
      std::filesystem::path(path), ec);
}

std::string join_root_rel(const std::string& root, const char* rel) {
  std::filesystem::path p(root);
  p /= rel;
  return p.lexically_normal().string();
}

}  // namespace

std::vector<std::string> sample_map_relative_paths() {
  return std::vector<std::string>(
      k_relative_candidates,
      k_relative_candidates +
          (sizeof(k_relative_candidates) / sizeof(k_relative_candidates[0])));
}

std::vector<std::string> resolve_sample_map_candidates(
    const std::vector<std::string>& search_roots) {
  std::vector<std::string> out;
  out.reserve(search_roots.size() *
              (sizeof(k_relative_candidates) / sizeof(k_relative_candidates[0])));
  for (const std::string& root : search_roots) {
    if (root.empty()) {
      continue;
    }
    for (const char* rel : k_relative_candidates) {
      out.push_back(join_root_rel(root, rel));
    }
  }
  return out;
}

bool try_resolve_existing_sample_map(
    const std::vector<std::string>& search_roots,
    std::string* out_path) {
  if (!out_path) {
    return false;
  }
  for (const std::string& cand : resolve_sample_map_candidates(search_roots)) {
    if (path_is_regular_file(cand)) {
      *out_path = cand;
      return true;
    }
  }
  return false;
}

}  // namespace content
