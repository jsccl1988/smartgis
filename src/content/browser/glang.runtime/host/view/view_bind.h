// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BIND_H_
#define IL_RUNTIME_BACKEND_VIEW_BIND_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


namespace detail {

// Generic view slots: tools, load facts, present timers.
// Scene slots are composed by register_scene_slots.
void register_view(content::ShellBridge& browser,
                   content::CapabilityHost* out,
                   const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_BIND_H_
