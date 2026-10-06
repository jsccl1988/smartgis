// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_
#define IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_

namespace app {

class Browser;

namespace detail {

// Synthetic pan/wheel burst for motion_gate. RMB must not be swallowed.
// Suite order (seed, still, stress, fps) lives in browse.il / browse.3d.il.
bool browse_stress(Browser& browser, const wchar_t* leaf, int count);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_BROWSE_STRESS_H_
