// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/ui_showcase.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/ui/layout/layout_gate.h"
#include "app/views/shell/harness/showcase/ui/present/present_capture.h"
#include "app/views/shell/harness/showcase/ui/present/scenario_panels.h"
#include "app/views/shell/harness/showcase/ui/seed/china_seed.h"
#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"

#include <cstdlib>
#include <windows.h>

namespace app {
namespace {

void showcase_mark(const char* token) {
  detail::write_mark(detail::kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

}  // namespace

int run_ui_showcase(Browser& browser, UiShowcaseMode mode) {
  if (mode == UiShowcaseMode::kNone) {
    return 0;
  }
  // Yellow identity HUD is opt-in; clear a stale process env so catalog/shell
  // BMPs are not polluted (visual_review bug 6).
  _putenv_s("SMT_MAP_IDENTITY_HUD", "0");
  // Scene DXGI present (WS_EX_NOREDIRECTIONBITMAP) makes PrintWindow of the
  // frame return a flat fill — prefer GDI placeholder for chrome BMPs.
  if (mode == UiShowcaseMode::kScene) {
    _putenv_s("SMT_PREFER_GDI_DEVICE", "1");
  }

  // Always stop present timers + detach before return (ExitProcess heap race).
  struct DetachOnExit {
    Browser& browser;
    ~DetachOnExit() { detail::detach_maps(browser); }
  } detach_guard{browser};

  detail::clear_mark(detail::kUiShowcaseMarkLeaf);
  showcase_mark(ui_showcase_name(mode));

  detail::apply_ui_harness_theme();
  showcase_mark("theme-ok");

  showcase_mark("pump-pre");
  pump_views_messages(600);
  showcase_mark("pump-post");
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    showcase_mark("hwnd-fail");
    return 2;
  }
  showcase_mark("hwnd-ok");

  // Replace demo stub with China carto before tab/chrome interaction + BMP.
  detail::ensure_ui_showcase_china_map(browser, mode);

  showcase_mark("interact-pre");
  detail::apply_ui_scenario_panels(browser, mode);
  showcase_mark("interact-post");

  if (const int layout_rc = detail::run_ui_layout_gate(browser, mode)) {
    return layout_rc;
  }

  return detail::run_ui_present_capture(browser, mode);
}

}  // namespace app
