// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_CAPABILITY_SESSION_BIND_H_
#define APP_VIEWS_RUNTIME_CAPABILITY_SESSION_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// Document, edit tools, digitize checks, and browse/wheel stress.
void bind_session(Browser& browser,
                  content::CapabilityHost* out,
                  const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_CAPABILITY_SESSION_BIND_H_
