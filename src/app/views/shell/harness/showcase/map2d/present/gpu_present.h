// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_GPU_PRESENT_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_GPU_PRESENT_H_

namespace content {
class Map2dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Optional FlyCube present_gpu cold+warm smoke (MAP2D_SHOWCASE_GPU=1).
// Skips quietly when unset or when no Device is available.
void run_optional_map2d_gpu_present(Browser& browser,
                                    content::Map2dPresenter* map2d,
                                    int showcase_w,
                                    int showcase_h);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_GPU_PRESENT_H_
