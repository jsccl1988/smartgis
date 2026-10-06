// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FINISH_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FINISH_H_

namespace app {

class Browser;

namespace detail {

// Borrowed shell FlyCube: KillTimer + pause_present only. Full detach of a
// live Display-thread Device races the next process (0xC000041D).
void finish_scene3d_showcase(Browser& browser, bool borrowed_shell);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FINISH_H_
