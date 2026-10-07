// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/view_host.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

void MapPagesComposer::attach_hwnd_gestures() {
  // Showcase / self-test set SKIP_AMBOX_CATALOG: HWND gesture subclass has
  // AVed under parallel ninja (std::function _Tidy on 0xcdcdcdcd). Product
  // ContentMapView / GDI overlay still needs attach so pan/pinch/right-click
  // hit input_hwnd() (do not skip on FORCE_CONTENT_MAPVIEW_2D).
  auto env_is_one = [](const char* name) {
    const char* v = base::switch_cstr(name);
    return v && v[0] == '1' && v[1] == '\0';
  };
  if (env_is_one("skip-ambox-catalog")) {
    return;
  }
  auto on_pinch = [this](int x, int y, double scale) {
    host_->browser_->handle_pinch(x, y, scale);
  };
  auto on_pan = [this](int dx, int dy) {
    host_->browser_->handle_gesture_pan(dx, dy);
  };
  // Only wire gestures for panes that already own a present device. Data/3D
  // are HWND-only until first tab focus (see attach_viewports / switch_map_tab).
  // Prefer input_hwnd() (FlyCube DXGI popup when visible) �?subclassing the
  // embed alone leaves pan/pinch/right-click dead under the present surface.
  auto try_attach = [&](ui::views::DrawHost* pane, content::MapHwndGestures* g) {
    if (!pane || !g || !host_->browser_) {
      return;
    }
    HWND hwnd = pane->input_hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return;
    }
    if (pane->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      return;
    }
    host_->browser_->session().attach_gestures(g, hwnd, on_pinch, on_pan);
    host_->configure_gestures(g);
  };
  try_attach(host_->map_edit_, host_->browser_->edit_gestures());
  try_attach(host_->map_scene_, host_->browser_->scene_gestures());
}


void MapPagesComposer::configure_gestures(content::MapHwndGestures* gestures) {
  if (!gestures || !host_->browser_) {
    return;
  }
  host_->browser_->session().configure_gestures(
      gestures,
      [this](HWND map_hwnd, int x, int y) {
        host_->on_map_right_click(map_hwnd, x, y);
      },
      [this](bool begin) { host_->browser_->on_extent_watch(begin); },
      [this]() { host_->browser_->refresh_scale(); });
}


}  // namespace app