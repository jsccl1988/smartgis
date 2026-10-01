// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/probe.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/public/map_contents.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>

namespace app {
namespace detail {

void pump_views_messages_impl(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (GetTickCount() < end &&
           PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

void self_test_detach_maps(Browser& browser) {
  detach_maps(browser);
}

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  if (!::app::detail::exe_capture_path(path, MAX_PATH, L"self-test-mark.txt")) {
    return;
  }
  static bool first = true;
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, first ? L"w" : L"a") == 0 && f) {
    first = false;
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
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
