// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_RUN_DISPATCH_H_
#define APP_VIEWS_HARNESS_RUN_DISPATCH_H_

#include <string_view>

namespace plugin {
class HarnessShell;
}

namespace app {

class Browser;

namespace detail {

// Publishes Browser as plugin::HarnessShell for the duration of |fn|.
int with_harness_shell(Browser& browser,
                       int (*fn)(plugin::HarnessShell&));

// Sends scenario identity to a product plugin command (payload optional JSON).
int dispatch_plugin_command(Browser& browser, const char* command_id,
                            std::string_view payload = {});

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_RUN_DISPATCH_H_
