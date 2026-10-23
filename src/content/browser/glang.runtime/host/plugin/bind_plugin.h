// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_
#define IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

namespace content {


namespace detail {

// Processing, scenario bodies, ResultPlayback, and the report dock.
void bind_plugin(content::Shell& browser, content::CapabilityHost* out);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_PLUGIN_BIND_H_
