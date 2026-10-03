// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_

#include <cstddef>

namespace app {

class Browser;

namespace detail {

// Resolve a leaf under exe (..\\data\\plugin\\ or data\\plugin\\).
bool resolve_stormsurge_sample(const wchar_t* leaf, char* out_utf8,
                               size_t out_cap);

// Resolve DEM + coast samples; marks stormsurge-sample-fail on miss.
bool resolve_stormsurge_inputs(char* dem_utf8, size_t dem_cap,
                               char* coast_utf8, size_t coast_cap);

// Resolve mask TIFF output path under exe (..\\data\\plugin\\stormsurge_mask.tif).
bool resolve_stormsurge_mask_output(char* out_utf8, size_t out_cap);

// load_coast + stormsurge.run; writes marks on failure.
bool seed_stormsurge_processing(Browser& browser, const char* dem_utf8,
                                const char* coast_utf8, const char* out_utf8);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_
