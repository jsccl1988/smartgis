// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_FRAMING_H_
#define PLUGIN_MAP2D_SCENARIO_FRAMING_H_

namespace plugin {

class HarnessShell;

namespace detail {

enum class ShowcaseMode;

int frame_map2d_showcase(HarnessShell& browser, ShowcaseMode mode,
                         int showcase_w, int showcase_h);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_FRAMING_H_
