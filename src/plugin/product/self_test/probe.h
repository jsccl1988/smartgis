// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_SELF_TEST_PROBE_H_
#define PLUGIN_PRODUCT_SELF_TEST_PROBE_H_

namespace ui {
namespace views {
class DrawHost;
}
}  // namespace ui

namespace plugin {

class SelfTestShell;

bool viewport_has_presented_frame(ui::views::DrawHost* pane);

// Resolve + canonicalize out/data china sample under the exe dir, then
// MapScene::open_path. |city_pack| is set when the chosen file is china_city.
bool try_open_china_sample(SelfTestShell& browser, bool* city_pack = nullptr);

int self_test_shell_ready(SelfTestShell& browser);
int self_test_edit_m0(SelfTestShell& browser);
int self_test_layers_m1(SelfTestShell& browser);
int self_test_navigate(SelfTestShell& browser);
int self_test_present(SelfTestShell& browser);
int self_test_layout_bounds(SelfTestShell& browser);
int self_test_milestones(SelfTestShell& browser);
int run_self_test_console(SelfTestShell& browser);

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_SELF_TEST_PROBE_H_
