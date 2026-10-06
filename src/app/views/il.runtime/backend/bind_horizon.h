// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// Shell, document, BMP export, and expect gates. One horizon pack.
void bind_horizon(Browser& browser,
                  content::CapabilityHost* out,
                  const wchar_t* mark_leaf);

void bind_export(Browser& browser, content::CapabilityHost* out);

void bind_expect(Browser& browser,
                 content::CapabilityHost* out,
                 const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_
