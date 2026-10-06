// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_GPU_PRESENT_H_
#define PLUGIN_MAP2D_SCENARIO_GPU_PRESENT_H_

namespace content {
class Map2dPresenter;
}  // namespace content

namespace plugin {
class HarnessShell;
namespace detail {

// Optional FlyCube present_gpu cold+warm smoke (MAP2D_SHOWCASE_GPU=1).
// Skips quietly when unset or when no Device is available.
void run_optional_map2d_gpu_present(HarnessShell& browser,
                                    content::Map2dPresenter* map2d,
                                    int showcase_w,
                                    int showcase_h);

}  // namespace detail

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_GPU_PRESENT_H_
