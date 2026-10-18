// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_LINK_BIND_HOST_H_
#define IL_RUNTIME_CAPABILITY_LINK_BIND_HOST_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


// Runtime link: fill |out| from the standard-library packs (capture, plugin,
// horizon, expect). Does not parse a script and does not choose a frontend.
// |mark_leaf| is the exe-sidecar mark file name. No-op if |out| is null.
void bind_host(content::ShellBridge& browser,
               content::CapabilityHost* out,
               const wchar_t* mark_leaf);

}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_LINK_BIND_HOST_H_
