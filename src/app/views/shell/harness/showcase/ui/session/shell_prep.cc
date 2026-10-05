// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/present/linger_policy.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {

void apply_ui_harness_theme() {
  const char* want = base::switch_cstr("ui-theme");
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
  // Drain queued WM_TIMER as well as KillTimer (see stop_map_present_timers).
  stop_map_present_timers(browser);
}

void force_ui_shell_repaint(Browser& browser) {
  if (ui::views::View* contents = browser.contents_view()) {
    if (ui::views::Widget* w = contents->widget()) {
      w->layout_contents();
    } else {
      contents->layout();
    }
  }
  if (ui::views::DrawHost* pane = browser.draw_host()) {
    pane->sync_native_bounds();
  }
  if (ui::views::DrawHost* pane = browser.data_draw_host()) {
    pane->sync_native_bounds();
  }
  if (ui::views::DrawHost* pane = browser.scene_draw_host()) {
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
  opts.timed_ms_env = "ui-showcase-timed-ms";
  opts.linger_ms_env = "ui-showcase-linger-ms";
  opts.linger_ms_zero_only = false;
  opts.default_until_close = false;
  return parse_linger_env(opts).ms;
}

}  // namespace detail
}  // namespace app
