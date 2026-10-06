// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_PROGRESS_H_
#define PLUGIN_MAP2D_SCENARIO_PROGRESS_H_

namespace plugin {

class HarnessShell;

namespace detail {

enum class ScenarioMode {
  kChina,
  kAlign,
  kOrthogrid,
};

inline constexpr int kMap2dDefaultW = 1280;
inline constexpr int kMap2dDefaultH = 720;

void bind_map2d_scenario_shell(HarnessShell* shell);
HarnessShell* map2d_scenario_shell();

void map2d_mark(const char* step);
void map2d_pixel_size(int* out_w, int* out_h);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_PROGRESS_H_
