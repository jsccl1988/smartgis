// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_CAPABILITY_SHELL_BIND_H_
#define APP_VIEWS_RUNTIME_CAPABILITY_SHELL_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// Pump, marks, tabs, window/key, and dialog suppression.
void bind_shell(Browser& browser,
                content::CapabilityHost* out,
                const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_CAPABILITY_SHELL_BIND_H_
