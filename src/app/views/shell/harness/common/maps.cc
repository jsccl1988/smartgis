// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/maps.h"

#include "app/views/shell/browser/browser.h"
#include "ui/views/map/map_viewport.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

void detach_maps(Browser& browser) {
  if (browser.scene3d()) {
    browser.scene3d()->abandon_mesh();
  }
  if (ui::views::MapViewport* m = browser.map_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_data_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_scene_viewport()) {
    m->detach();
  }
}

void stop_map_present_timers(Browser& browser) {
  auto stop = [](ui::views::MapViewport* pane) {
    if (pane && pane->native_view() && IsWindow(pane->native_view())) {
      KillTimer(pane->native_view(), 1);
    }
  };
  stop(browser.map_viewport());
  stop(browser.map_data_viewport());
  stop(browser.map_scene_viewport());
}

}  // namespace detail
}  // namespace app
