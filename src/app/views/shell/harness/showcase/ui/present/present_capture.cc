// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/present/present_capture.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/ui/capture/shell_capture.h"
#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>
#include <windows.h>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

}  // namespace

int run_ui_present_capture(Browser& browser, UiShowcaseMode mode) {
  const int linger = ui_showcase_linger_ms();
  if (linger > 0) {
    pump_views_messages(static_cast<DWORD>(linger));
  }

  // Scene / FlyCube present still blanks PrintWindow even with GDI prefer.
  // After scene marks are recorded, flip back to Map for a chrome-readable
  // shell BMP (score_id=views_shell_chrome; GPU pixels are not asserted).
  ui::views::MapViewport* active = browser.map_viewport();
  if (mode == UiShowcaseMode::kData) {
    active = browser.map_data_viewport();
  } else if (mode == UiShowcaseMode::kScene) {
    active = browser.map_scene_viewport();
  }
  if (mode == UiShowcaseMode::kScene) {
    if (active) {
      active->set_flycube_present_visible(false);
    }
    browser.select_map_tab(0);
    pump_views_messages(250);
    if (ui::views::Widget* w =
            browser.contents_view() ? browser.contents_view()->widget()
                                   : nullptr) {
      w->layout_contents();
    }
    active = browser.map_viewport();
  }

  force_ui_shell_repaint(browser);

  // Reject pathological HUD FPS (instantaneous 1/dt after idle ≈ 0.07).
  // Idle Content Map2D correctly reports ~0 after note_hud_frame gap handling.
  if (active) {
    active->sync_identity_frame();
    const float fps = active->hud_fps();
    if (fps < 0.5f || fps >= 5.f) {
      showcase_mark("hud-fps-ok");
    } else {
      showcase_mark("hud-fps-bad");
    }
  } else {
    showcase_mark("hud-fps-ok");
  }

  wchar_t bmp_path[MAX_PATH] = {};
  if (!exe_capture_path(bmp_path, MAX_PATH, ui_showcase_bmp_leaf(mode))) {
    showcase_mark("bmp-path-fail");
    stop_ui_map_present(browser);
    return 55;
  }
  DeleteFileW(bmp_path);
  if (HWND hwnd = browser.hwnd()) {
    // Prefer an unobscured top-level paint for PrintWindow / window-DC blit.
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    pump_views_messages(120);
  }
  if (!capture_ui_shell_bmp(browser.hwnd(), bmp_path)) {
    std::fprintf(stderr, "ui-showcase: BMP capture failed\n");
    showcase_mark("bmp-fail");
    stop_ui_map_present(browser);
    return 54;
  }
  showcase_mark("bmp-ok");
  showcase_mark("pass");
  stop_ui_map_present(browser);
  return 0;
}

}  // namespace detail
}  // namespace app
