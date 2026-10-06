// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_
#define IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// Processing, showcase bodies, ResultPlayback, and the report dock.
void bind_plugin(Browser& browser, content::CapabilityHost* out);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_
