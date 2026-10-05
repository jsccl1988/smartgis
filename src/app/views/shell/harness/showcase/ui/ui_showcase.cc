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
#include "base/process/switches.h"

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
  // Match product: identity HUD (engine + FPS) stays on. Do not skip FlyCube
  // for scene BMPs — GDI placeholder is not the product 3D face.
  base::set_switch("map-identity-hud", "1");
  if (mode == UiShowcaseMode::kScene) {
    base::set_switch("prefer-gdi-device", "0");
  }

  // Teardown: always KillTimer + drain WM_TIMER. Full DrawHost::detach of a
  // live session races TerminateProcess (STATUS_HEAP_CORRUPTION 0xC0000374)
  // after a green BMP — skip detach on rc==0 (exit_after_scenario kills the
  // process). Keep detach on failure so callers that return into run_loop
  // do not leave present ticks attached. Do not use KillTimer-only
  // stop_ui_map_present here (queued WM_TIMER still fires).
  struct ExitTeardown {
    Browser& browser;
    bool detach_live_hosts = true;
    ~ExitTeardown() {
      if (detach_live_hosts) {
        detail::detach_maps(browser);
      } else {
        detail::stop_map_present_timers(browser);
      }
    }
  } teardown{browser};

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

  const int rc = detail::run_ui_present_capture(browser, mode);
  if (rc == 0) {
    teardown.detach_live_hosts = false;
  }
  return rc;
}

}  // namespace app
