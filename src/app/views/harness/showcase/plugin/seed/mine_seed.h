// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MINE_SEED_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MINE_SEED_H_

#include <cstddef>

namespace app {

class Browser;

namespace detail {

// Resolve mine_boreholes.csv under exe (..\\data\\plugin or data\\plugin).
bool resolve_mine_boreholes_csv(char* out_utf8, size_t out_cap);

// Run mine.interpolate_stratum + mine.prism_volume; writes marks on failure.
// Returns false when plugins are missing or either processing call fails.
bool seed_mine_processing(Browser& browser, const char* csv_utf8);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MINE_SEED_H_
