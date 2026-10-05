// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_BOOTSTRAP_H_
#define CONTENT_PUBLIC_MAP_BOOTSTRAP_H_

#include <string>
#include <vector>

#include "content/content_export.h"

namespace content {

// HWND-free sample / china map path policy (SP3 host extract).
// Leftover App and Views open paths share these helpers; opening GDAL /
// registering leftover mapmgr stays at the call site.

// Ordered relative paths for the product China pack (SoT for bare launch,
// MapScene::seed_default, and harness try_open_china_sample).
// All china_city.* candidates precede any china_plp.* fallback. Prefers
// shared `out/data/` (exe under out/Debug|Release → `../data/`), then
// `data/` next to the exe, flat next-to-exe, and testing/data fallbacks.
CONTENT_EXPORT std::vector<std::string> sample_map_relative_paths();

// Relative candidates under each search root, preferred order first.
// Roots may include a trailing separator.
CONTENT_EXPORT std::vector<std::string> resolve_sample_map_candidates(
    const std::vector<std::string>& search_roots);

// Pick the first existing candidate file. Returns false if none exist.
CONTENT_EXPORT bool try_resolve_existing_sample_map(
    const std::vector<std::string>& search_roots,
    std::string* out_path);

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_BOOTSTRAP_H_
