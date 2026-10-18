// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/gate.h"

#include <string_view>

#include "plugin/product/map2d/seed/seed.h"

namespace plugin {

bool map2d_gate_contract(void*) {
  Map2dSeedMode mode = Map2dSeedMode::kChina;
  return parse_map2d_seed_mode("china", &mode) &&
         mode == Map2dSeedMode::kChina &&
         parse_map2d_seed_mode("align", &mode) &&
         mode == Map2dSeedMode::kAlign &&
         parse_map2d_seed_mode("baogrid", &mode) &&
         mode == Map2dSeedMode::kOrthogrid &&
         !parse_map2d_seed_mode("nope", &mode);
}

bool map2d_gate_compute(void*) {
  return std::string_view(map2d_seed_mode_name(Map2dSeedMode::kChina)) ==
             "china" &&
         std::string_view(map2d_seed_mode_name(Map2dSeedMode::kAlign)) ==
             "align" &&
         std::string_view(map2d_seed_mode_name(Map2dSeedMode::kOrthogrid)) ==
             "orthogrid";
}

}  // namespace plugin
