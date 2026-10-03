// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/seed/framing.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"

#include <cstdio>

namespace app {
namespace detail {

int frame_map2d_showcase(Browser& browser,
                         Map2dShowcaseMode mode,
                         int showcase_w,
                         int showcase_h) {
  // Frame ViewFrame to export pixels â€?not the live HWND client size.
  // fit_map_extent() uses Map Edit client (~2k wide); export_bmp would then
  // sample only the NW 640x480 of that pan (often ocean).
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    std::fprintf(stderr, "map2d-showcase: ViewFrame missing\n");
    return 57;
  }
  if (mode == Map2dShowcaseMode::kOrthogrid) {
    // Explicit lon/lat framing (unit square). fit_extent alone has historically
    // left the ViewFrame on China framing when polygon MBR / Y-flip disagree.
    constexpr content::Extent2 kOrthogridFraming{0.0, 0.0, 1.0, 1.0};
    frame->apply_world_extent(kOrthogridFraming, showcase_w, showcase_h);
    double minx = 0, miny = 0, maxx = 0, maxy = 0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy)) {
      std::fprintf(stderr,
                   "map2d-showcase: orthogrid extent=(%.3f,%.3f)-(%.3f,%.3f) "
                   "features=%zu scale=%g\n",
                   minx, miny, maxx, maxy, browser.document()->feature_count(),
                   frame->scale());
    }
    if (browser.map2d()) {
      browser.map2d()->invalidate_frame_cache();
    }
  } else {
    // Shared mainland framing with interactive China product defaults
    // (keep align StyleDocument â€?do not clear_style here).
    frame_china_map2d(browser, showcase_w, showcase_h);
  }
  return 0;
}

}  // namespace detail
}  // namespace app
