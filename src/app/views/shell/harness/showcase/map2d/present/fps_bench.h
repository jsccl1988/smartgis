// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_FPS_BENCH_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_FPS_BENCH_H_

namespace content {
class Map2dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Optional HUD FPS sample loop (MAP2D_FPS_BENCH_MS). Writes
// map2d-fps-bench.txt under the exe capture dir when enabled.
void run_optional_map2d_fps_bench(Browser& browser,
                                  content::Map2dPresenter* map2d);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_FPS_BENCH_H_
