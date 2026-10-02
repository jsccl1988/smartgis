// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/mode_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/showcase/map2d/orthogrid_mesh.h"
#include "app/views/shell/harness/showcase/map2d/sample.h"
#include "content/browser/document/map_scene.h"

#include <cstdio>

namespace app {
namespace detail {

int seed_map2d_mode(Browser& browser, Map2dShowcaseMode mode) {
  if (mode != Map2dShowcaseMode::kChina && mode != Map2dShowcaseMode::kAlign &&
      mode != Map2dShowcaseMode::kOrthogrid) {
    std::fprintf(stderr, "map2d-showcase: unsupported mode\n");
    return 53;
  }

  if (mode == Map2dShowcaseMode::kOrthogrid) {
    if (!load_map2d_orthogrid_mesh(browser)) {
      std::fprintf(stderr, "map2d-showcase: orthogrid mesh failed\n");
      return 55;
    }
    map2d_showcase_mark("orthogrid-ok");
    return 0;
  }

  if (mode == Map2dShowcaseMode::kAlign) {
    // Same china_city pack as --map2d-showcase=china / main-app Open.
    if (!try_open_china_sample(browser)) {
      if (browser.document()) {
        browser.document()->seed_default();
      }
    }
    if (!browser.document() || browser.document()->feature_count() < 3) {
      std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
      return 55;
    }
    map2d_showcase_mark("china-ok");
    if (!try_load_align_style(browser)) {
      std::fprintf(stderr, "map2d-showcase: style_align.json load failed\n");
      return 55;
    }
    return 0;
  }

  // kChina
  if (!try_open_china_sample(browser)) {
    // Fall back to MapScene seed (china_city / stub).
    if (browser.document()) {
      browser.document()->seed_default();
    }
  }
  if (!browser.document() || browser.document()->feature_count() < 3) {
    std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
    return 55;
  }
  map2d_showcase_mark("china-ok");
  // Same carto clear as interactive fit_map_extent / seed_default.
  ensure_china_maplibre_carto(browser);
  map2d_showcase_mark("style-carto-default");
  return 0;
}

}  // namespace detail
}  // namespace app
