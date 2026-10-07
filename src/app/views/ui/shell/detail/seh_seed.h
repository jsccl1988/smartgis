// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_SHELL_DETAIL_SEH_SEED_H_
#define APP_VIEWS_UI_SHELL_DETAIL_SEH_SEED_H_

namespace app {

class Browser;

namespace detail {

// SEH wrappers for china OGR seed / fit during init_shell. Keep C++ objects
// out of the __try frames (MSVC C2712) — bodies live in seh_seed.cc.

bool seh_seed_default(Browser* browser, bool allow_china);
bool seh_fit_and_push_extent(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UI_SHELL_DETAIL_SEH_SEED_H_
