// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_RUN_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_RUN_H_

namespace app {

class Browser;

namespace detail {

// Layout warm, software BMP capture, optional GPU present smoke, optional FPS
// bench. Returns showcase exit code (0 / 54 / 56 / 57). Does not detach maps.
int run_map2d_present(Browser& browser,
                      const char* mode_name,
                      int showcase_w,
                      int showcase_h);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_PRESENT_RUN_H_
