// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/probe.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "content/public/map_contents.h"
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {

void pump_views_messages_impl(DWORD ms) {
  pump_messages(ms);
}

void self_test_detach_maps(Browser& browser) {
  detach_maps(browser);
}

void self_test_mark(const char* step) {
  write_mark(kSelfTestMarkLeaf, step, /*truncate=*/true);
}

bool viewport_has_presented_frame(ui::views::MapViewport* pane) {
  if (!pane ||
      pane->attach_mode() !=
          ui::views::MapViewport::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

}  // namespace detail
}  // namespace app
