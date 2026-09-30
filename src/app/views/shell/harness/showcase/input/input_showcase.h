// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_INPUT_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_INPUT_SHOWCASE_H_

namespace app {

class Browser;

// Lean digitize / FeatureGeom gate for testing/tools/case/input_loop.py.
// Stays on the Map Edit tab (no Data/3D switch). Writes
// input-self-test-mark.txt next to the exe.
int run_input_showcase(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_INPUT_SHOWCASE_H_
