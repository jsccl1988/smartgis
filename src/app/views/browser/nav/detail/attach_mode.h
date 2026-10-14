// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_NAV_DETAIL_ATTACH_MODE_H_
#define APP_VIEWS_BROWSER_NAV_DETAIL_ATTACH_MODE_H_

#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {

// True when the active DrawHost is ContentMapView (no StretchBlt pan preview).
inline bool is_content_mapview(const ui::views::DrawHost* pane) {
  return pane &&
         pane->attach_mode() ==
             ui::views::DrawHost::AttachMode::kContentMapView;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_NAV_DETAIL_ATTACH_MODE_H_
