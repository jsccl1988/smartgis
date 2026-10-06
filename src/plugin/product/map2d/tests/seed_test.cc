// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/seed/seed.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

void expect(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "map2d_seed_test FAIL: %s\n", what);
    std::exit(1);
  }
}

}  // namespace

int main() {
  plugin::Map2dSeedMode mode = plugin::Map2dSeedMode::kChina;
  expect(plugin::parse_map2d_seed_mode("china", &mode) &&
             mode == plugin::Map2dSeedMode::kChina,
         "china");
  expect(plugin::parse_map2d_seed_mode("align", &mode) &&
             mode == plugin::Map2dSeedMode::kAlign,
         "align");
  expect(plugin::parse_map2d_seed_mode("baogrid", &mode) &&
             mode == plugin::Map2dSeedMode::kOrthogrid,
         "baogrid");
  expect(!plugin::parse_map2d_seed_mode("nope", &mode), "reject");
  expect(std::string_view(plugin::map2d_seed_mode_name(
             plugin::Map2dSeedMode::kOrthogrid)) == "orthogrid",
         "name");
  std::fprintf(stderr, "map2d_seed_test PASS\n");
  return 0;
}
