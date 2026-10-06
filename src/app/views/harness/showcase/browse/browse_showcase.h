// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHOWCASE_BROWSE_BROWSE_SHOWCASE_H_
#define APP_VIEWS_SHOWCASE_BROWSE_BROWSE_SHOWCASE_H_

namespace app {

class Browser;

// Lean pan / browse-stress / wheel-cursor gate (suite "browse").
// Prefers testing/tools/harness/shell/browse/browse.il; falls back to C++ navigate.
int run_browse_showcase(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_SHOWCASE_BROWSE_BROWSE_SHOWCASE_H_
