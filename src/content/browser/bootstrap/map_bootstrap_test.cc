// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/map_bootstrap.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path tmp =
      fs::temp_directory_path() / "smartgis_map_bootstrap_test";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  // Mimic out/Debug exe with shared out/data sibling.
  const fs::path exe_root = tmp / "Debug";
  const fs::path data_root = tmp / "data";
  fs::create_directories(exe_root, ec);
  expect(!ec, "create temp Debug dir");
  fs::create_directories(data_root, ec);
  expect(!ec, "create temp data dir");

  const fs::path gpkg = data_root / "china_city.gpkg";
  {
    std::ofstream f(gpkg.string(), std::ios::binary);
    f << "stub";
  }
  expect(fs::is_regular_file(gpkg), "touch ../data/china_city.gpkg");

  const std::string root = exe_root.string() + "/";
  auto cands = content::resolve_sample_map_candidates({root});
  expect(!cands.empty(), "candidates non-empty");
  expect(cands.front().find("china_city") != std::string::npos,
         "first candidate names china_city");

  std::string chosen;
  expect(content::try_resolve_existing_sample_map({root}, &chosen),
         "resolve existing sample");
  expect(chosen.find("china_city.gpkg") != std::string::npos,
         "chose china_city.gpkg");
  // Chosen path should be the shared sibling data/ (lexically normalized).
  expect(chosen.find("data") != std::string::npos, "chose under data/");

  fs::remove_all(tmp, ec);
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}
