// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_FPS_BENCH_H_
#define PLUGIN_MAP2D_SCENARIO_FPS_BENCH_H_

namespace content {
class Map2dPresenter;
}  // namespace content

namespace plugin {
class HarnessShell;
namespace detail {

// Optional HUD FPS sample loop (MAP2D_FPS_BENCH_MS). Writes
// map2d-fps-bench.txt under the exe capture dir when enabled.
void run_optional_map2d_fps_bench(HarnessShell& browser,
                                  content::Map2dPresenter* map2d);

}  // namespace detail

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_FPS_BENCH_H_
