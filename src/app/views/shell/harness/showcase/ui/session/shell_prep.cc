// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/present/linger_policy.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/map_viewport.h"

#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace app {
namespace detail {

void apply_ui_harness_theme() {
  const char* want = std::getenv("SMT_UI_THEME");
  const char* id = "dark";
  if (want && want[0]) {
    if (std::strcmp(want, "light") == 0) {
      id = "light";
    } else if (std::strcmp(want, "dark") == 0) {
      id = "dark";
    }
  }
  ui::views::ThemeService::get().ensure_builtin_packs();
  ui::views::ThemeService::get().set_theme(id, /*persist_to_disk=*/false);
}

void stop_ui_map_present(Browser& browser) {
  auto stop = [](ui::views::MapViewport* pane) {
    if (pane && pane->native_view() && IsWindow(pane->native_view())) {
      KillTimer(pane->native_view(), 1);
    }
  };
  stop(browser.map_viewport());
  stop(browser.map_data_viewport());
  stop(browser.map_scene_viewport());
}

void force_ui_shell_repaint(Browser& browser) {
  if (ui::views::View* contents = browser.contents_view()) {
    if (ui::views::Widget* w = contents->widget()) {
      w->layout_contents();
    } else {
      contents->layout();
    }
  }
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    pane->sync_native_bounds();
  }
  if (ui::views::MapViewport* pane = browser.map_data_viewport()) {
    pane->sync_native_bounds();
  }
  if (ui::views::MapViewport* pane = browser.map_scene_viewport()) {
    pane->sync_native_bounds();
  }
  if (HWND hwnd = browser.hwnd()) {
    for (int i = 0; i < 4; ++i) {
      InvalidateRect(hwnd, nullptr, TRUE);
      UpdateWindow(hwnd);
      pump_views_messages(160);
    }
  } else {
    pump_views_messages(300);
  }
}

int ui_showcase_linger_ms() {
  LingerEnvOpts opts;
  opts.timed_ms_env = "SMT_UI_SHOWCASE_TIMED_MS";
  opts.linger_ms_env = "SMT_UI_SHOWCASE_LINGER_MS";
  opts.linger_ms_zero_only = false;
  opts.default_until_close = false;
  return parse_linger_env(opts).ms;
}

}  // namespace detail
}  // namespace app
