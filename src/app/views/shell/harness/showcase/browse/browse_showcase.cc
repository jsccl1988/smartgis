// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/browse/browse_showcase.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/bmp.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/self_test/probe.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "ui/views/map/map_viewport.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

// Suite id for Interact IL: SMT_HARNESS_SUITE, else browse.3d when the
// SMT_UI_INTERACT_SCRIPT leaf names it, else "browse".
const char* resolve_browse_suite_id() {
  if (const char* env = std::getenv("SMT_HARNESS_SUITE")) {
    if (env[0]) {
      return env;
    }
  }
  if (const char* script = std::getenv("SMT_UI_INTERACT_SCRIPT")) {
    if (std::strstr(script, "browse.3d")) {
      return "browse.3d";
    }
  }
  return "browse";
}

bool is_browse_3d_suite(const char* suite_id) {
  return suite_id && std::strcmp(suite_id, "browse.3d") == 0;
}

// Prefer FlyCube present HWND (input_hwnd): shell PrintWindow cannot sample
// WS_EX_NOREDIRECTIONBITMAP DXGI flip contents (hollow navy / sheared chrome).
void capture_browse_shell_bmp(Browser& browser, bool is_3d) {
  if (is_3d) {
    browser.select_map_tab(2);
  } else {
    browser.select_map_tab(0);
  }
  ui::views::MapViewport* pane =
      is_3d ? browser.map_scene_viewport() : browser.map_viewport();
  // browse.il stops present timers before stress; wake a frame for capture.
  if (pane) {
    pane->set_flycube_present_visible(true);
    pane->invalidate_native();
  }
  detail::pump_messages(400);

  HWND hwnd = nullptr;
  if (pane) {
    hwnd = pane->input_hwnd();
  }
  if (!hwnd || !IsWindow(hwnd)) {
    hwnd = browser.hwnd();
  }
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  wchar_t path[MAX_PATH] = {};
  const wchar_t* leaf =
      is_3d ? L"browse-showcase-3d.bmp" : L"browse-showcase-2d.bmp";
  if (!detail::exe_capture_path(path, MAX_PATH, leaf)) {
    return;
  }
  detail::CaptureOpts opts;
  opts.max_attempts = 6;
  opts.pump_base_ms = 80;
  opts.pump_step_ms = 50;
  opts.require_chrome_diversity = false;
  opts.visible = detail::VisiblePolicy::kGridLitFraction;
  if (detail::capture_hwnd_bmp(hwnd, path, opts)) {
    detail::self_test_mark(is_3d ? "bmp3d-ok" : "bmp-ok");
  }
}

}  // namespace

int run_browse_showcase(Browser& browser) {
  detail::clear_mark(detail::kSelfTestMarkLeaf);
  detail::pump_messages(300);
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    detail::detach_maps(browser);
    return 2;
  }
  detail::self_test_mark("show");
  detail::self_test_mark("hwnd-ok");

  const char* suite_id = resolve_browse_suite_id();
  // browse.3d.il selects Map tab 2 itself; forcing tab 0 first races FlyCube
  // attach / abandon under dual-pane and has exited -1 with empty marks.
  if (!is_browse_3d_suite(suite_id)) {
    browser.select_map_tab(0);
    detail::pump_messages(400);
  } else {
    detail::self_test_mark("suite-browse3d");
    detail::pump_messages(200);
  }

  if (try_run_suite_script(browser, suite_id, detail::kSelfTestMarkLeaf,
                           /*clear_marks=*/false)) {
    // Capture while present threads still paint — stop_map_present_timers first
    // yields a hollow navy FlyCube frame (ui_shell_dark accent=0).
    detail::pump_messages(150);
    capture_browse_shell_bmp(browser, is_browse_3d_suite(suite_id));
    detail::stop_map_present_timers(browser);
    detail::pump_messages(50);
    return 0;
  }

  // Script failed mid-way (incomplete marks). Keep maps attached for the C++
  // navigate fallback so pan/browse/wheel marks can still land; detach after.
  // browse.3d has no 2D navigate fallback that produces browse3d-* marks.
  if (is_browse_3d_suite(suite_id)) {
    detail::self_test_mark("dsl-fail");
    detail::pump_messages(100);
    capture_browse_shell_bmp(browser, true);
    detail::stop_map_present_timers(browser);
    return 3;
  }
  detail::self_test_mark("dsl-fallback");
  const int rc = detail::self_test_navigate(browser);
  detail::pump_messages(100);
  capture_browse_shell_bmp(browser, false);
  detail::stop_map_present_timers(browser);
  return rc;
}

}  // namespace app
