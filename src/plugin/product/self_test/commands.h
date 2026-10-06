// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_SELF_TEST_COMMANDS_H_
#define PLUGIN_PRODUCT_SELF_TEST_COMMANDS_H_

namespace content {
class PluginHost;
}

namespace plugin {

class SelfTestShell;

// Builtin pack: contribute self_test.* command ids + processing id.
bool register_self_test(content::PluginHost* host);

// Last milestone exit code after execute("self_test.*") (0 = pass).
int self_test_last_exit_code();

int run_self_test_all(SelfTestShell& browser);
int run_self_test_console(SelfTestShell& browser);
int self_test_shell_ready(SelfTestShell& browser);
int self_test_edit_m0(SelfTestShell& browser);
int self_test_layers_m1(SelfTestShell& browser);
int self_test_navigate(SelfTestShell& browser);
int self_test_present(SelfTestShell& browser);
int self_test_layout_bounds(SelfTestShell& browser);
int self_test_milestones(SelfTestShell& browser);

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_SELF_TEST_COMMANDS_H_
