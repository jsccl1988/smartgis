// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/framing.h"

#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/runtime/host/capability/shell.h"
#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"

#include <cstdio>

namespace plugin {
namespace detail {
namespace {

void frame_china_map2d(HarnessShell& browser, int view_w, int view_h) {
  content::GisScene* doc = browser.document();
  content::ViewFrame* frame = browser.view_frame();
  if (!doc || !frame) {
    return;
  }
  if (doc->has_china_extent()) {
    frame->apply_world_extent(content::kChinaMap2dFrameExtent, view_w, view_h);
    if (content::OrbitFrame* orbit = browser.orbit_frame()) {
      orbit->apply_world_extent(content::kChinaLonLatExtent);
    }
  } else {
    frame->fit_extent(*doc, view_w, view_h);
    if (content::OrbitFrame* orbit = browser.orbit_frame()) {
      orbit->apply_world_extent(doc->world_extent());
    }
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  browser.push_shared_extent();
}

}  // namespace

int frame_map2d_showcase(HarnessShell& browser, ScenarioMode mode,
                         int showcase_w, int showcase_h) {
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    std::fprintf(stderr, "map2d-showcase: ViewFrame missing\n");
    return 57;
  }
  if (mode == ScenarioMode::kOrthogrid) {
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
    frame_china_map2d(browser, showcase_w, showcase_h);
  }
  return 0;
}

}  // namespace detail
}  // namespace plugin
