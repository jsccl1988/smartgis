// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


namespace detail {

// Horizon slots only: clock, inject, shell UI, and shell gates.
void register_horizon(content::ShellBridge& browser,
                      content::CapabilityHost* out,
                      const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_BIND_H_
