// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_PRESENT_RUN_H_
#define PLUGIN_MAP2D_SCENARIO_PRESENT_RUN_H_

namespace plugin {
class HarnessShell;
namespace detail {

// Layout warm, software BMP capture, optional GPU present smoke, optional FPS
// bench. Returns showcase exit code (0 / 54 / 56 / 57). Does not detach maps.
int run_map2d_present(HarnessShell& browser,
                      const char* mode_name,
                      int showcase_w,
                      int showcase_h);

}  // namespace detail

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_PRESENT_RUN_H_
