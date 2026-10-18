// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scene capability wiring. Generic view slots stay in view_bind.

#ifndef IL_RUNTIME_HOST_SCENE_BIND_H_
#define IL_RUNTIME_HOST_SCENE_BIND_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {
namespace detail {

void register_scene_slots(content::ShellBridge& browser,
                          content::CapabilityHost* out,
                          const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_HOST_SCENE_BIND_H_
