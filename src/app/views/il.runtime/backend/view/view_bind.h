// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BIND_H_
#define IL_RUNTIME_BACKEND_VIEW_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// View slots: present, tools, load facts, and view gates.
void register_view(Browser& browser,
                   content::CapabilityHost* out,
                   const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_BIND_H_
