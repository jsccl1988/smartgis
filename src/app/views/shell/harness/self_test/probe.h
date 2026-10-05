// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SELF_TEST_PROBE_H_
#define APP_VIEWS_SHELL_HARNESS_SELF_TEST_PROBE_H_

#include <windows.h>

namespace ui {
namespace views {
class DrawHost;
}
}  // namespace ui

namespace app {

class Browser;

namespace detail {

// Shared --self-test helpers (mark sidecar, detach, message pump).
void pump_views_messages_impl(DWORD ms);
void self_test_detach_maps(Browser& browser);
void self_test_mark(const char* step);
bool viewport_has_presented_frame(ui::views::DrawHost* pane);

// Stage returns: 0 = continue, non-zero = process exit code.
int self_test_shell_ready(Browser& browser);
int self_test_edit_m0(Browser& browser);
int self_test_layers_m1(Browser& browser);
int self_test_navigate(Browser& browser);
int self_test_present(Browser& browser);
int self_test_layout_bounds(Browser& browser);
int self_test_milestones(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SELF_TEST_PROBE_H_
