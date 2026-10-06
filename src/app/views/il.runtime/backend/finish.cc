// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/finish.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {

void finish_scene3d_showcase(Browser& browser, bool borrowed_shell) {
  if (!borrowed_shell) {
    detach_maps(browser);
    return;
  }
  stop_map_present_timers(browser);
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    scene->pause_present();
  }
}

}  // namespace detail
}  // namespace app
