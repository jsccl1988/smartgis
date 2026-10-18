// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_
#define IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


namespace detail {

// Synthetic pan/wheel burst for motion_gate. RMB must not be swallowed.
// Suite order (seed, still, stress, fps) lives in the suite .py scripts.
bool browse_stress(content::ShellBridge& browser, const wchar_t* leaf, int count);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_
