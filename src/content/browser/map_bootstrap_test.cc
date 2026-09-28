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
  fs::create_directories(tmp, ec);
  expect(!ec, "create temp dir");

  const fs::path gpkg = tmp / "china_city.gpkg";
  {
    std::ofstream f(gpkg.string(), std::ios::binary);
    f << "stub";
  }
  expect(fs::is_regular_file(gpkg), "touch china_city.gpkg");

  const std::string root = tmp.string() + "/";
  auto cands = content::resolve_sample_map_candidates({root});
  expect(!cands.empty(), "candidates non-empty");
  expect(cands.front().find("china_city") != std::string::npos,
         "first candidate prefers china_city");

  std::string chosen;
  expect(content::try_resolve_existing_sample_map({root}, &chosen),
         "resolve existing sample");
  expect(chosen.find("china_city.gpkg") != std::string::npos,
         "chose china_city.gpkg");

  fs::remove_all(tmp, ec);
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}
