// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/self_test/commands.h"

#include "plugin/product/self_test/probe.h"
#include "plugin/product/self_test/shell.h"

#include "base/trace/event/process_trace.h"

#include <windows.h>

namespace plugin {

int run_self_test_all(SelfTestShell& browser) {
  struct TraceDumpOnExit {
    ~TraceDumpOnExit() { base::trace::maybe_dump_tracing_to_env(); }
  } trace_dump_on_exit;
  (void)trace_dump_on_exit;

  wchar_t mark_path[MAX_PATH] = {};
  if (browser.capture_path(mark_path, MAX_PATH, L"self-test-mark.txt")) {
    DeleteFileW(mark_path);
  }

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

}  // namespace plugin
