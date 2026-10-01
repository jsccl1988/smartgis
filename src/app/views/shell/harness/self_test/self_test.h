// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SELF_TEST_H_
#define APP_VIEWS_SHELL_SELF_TEST_H_

#include <windows.h>

namespace app {
class Browser;
int run_views_self_test(Browser& browser);
// Shorter DebugAgent console path (--self-test-console): wire host hooks,
// run :help/:layers/:extent/:refresh, pan timing, write console_bench.json.
// Prefers console.il via CapabilityHost; falls back to body.
int run_views_console_self_test(Browser& browser);
// C++ body used by Host console_run and as script fallback.
int console_self_test_body(Browser& browser);
void pump_views_messages(DWORD ms);
}  // namespace app

#endif
