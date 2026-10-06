// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_MINE_SCENARIO_SEED_H_
#define PLUGIN_PRODUCT_MINE_SCENARIO_SEED_H_

#include <cstddef>

namespace plugin {

class HarnessShell;

namespace detail {

// Resolve mine_boreholes.csv under exe (..\\data\\plugin or data\\plugin).
bool resolve_mine_boreholes_csv(char* out_utf8, size_t out_cap);

// Run mine.interpolate_stratum + mine.prism_volume; writes marks on failure.
// Returns false when plugins are missing or either processing call fails.
bool seed_mine_processing(HarnessShell& browser, const char* csv_utf8);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_MINE_SCENARIO_SEED_H_
