// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_PROGRESS_H_
#define PLUGIN_MAP2D_SCENARIO_PROGRESS_H_

// Product-specific map2d showcase metrics. TLS shell binders / marks live in
// host capability/scenario_shell.h (bind_map2d_scenario_shell / map2d_mark).
#include "plugin/runtime/host/capability/scenario_shell.h"

namespace plugin {
namespace detail {

enum class ScenarioMode {
  kChina,
  kAlign,
  kOrthogrid,
};

inline constexpr int kMap2dDefaultW = 1280;
inline constexpr int kMap2dDefaultH = 720;

void map2d_pixel_size(int* out_w, int* out_h);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_PROGRESS_H_
