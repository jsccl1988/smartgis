// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/self_test.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/self_test/probe.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/trace/event/process_trace.h"

#include <cstdio>

namespace app {

int run_views_self_test(Browser& browser) {
  struct TraceDumpOnExit {
    ~TraceDumpOnExit() { base::trace::maybe_dump_tracing_to_env(); }
  } trace_dump_on_exit;
  (void)trace_dump_on_exit;

  wchar_t mark_path[MAX_PATH] = {};
  if (detail::exe_capture_path(mark_path, MAX_PATH, L"self-test-mark.txt")) {
    DeleteFileW(mark_path);
  }

  using detail::self_test_shell_ready;
  using detail::self_test_edit_m0;
  using detail::self_test_layers_m1;
  using detail::self_test_layout_bounds;
  using detail::self_test_milestones;
  using detail::self_test_navigate;
  using detail::self_test_present;

  if (int rc = self_test_shell_ready(browser)) {
    return rc;
  }
  if (int rc = self_test_edit_m0(browser)) {
    return rc;
  }
  if (int rc = self_test_layers_m1(browser)) {
    return rc;
  }
  if (int rc = self_test_navigate(browser)) {
    return rc;
  }
  if (int rc = self_test_present(browser)) {
    return rc;
  }
  if (int rc = self_test_layout_bounds(browser)) {
    return rc;
  }
  return self_test_milestones(browser);
}

void pump_views_messages(DWORD ms) { detail::pump_messages(ms); }

}  // namespace app
