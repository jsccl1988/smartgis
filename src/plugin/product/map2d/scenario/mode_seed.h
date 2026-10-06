// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_MODE_SEED_H_
#define PLUGIN_MAP2D_SCENARIO_MODE_SEED_H_

namespace plugin {

class HarnessShell;

namespace detail {

enum class ShowcaseMode;

int seed_map2d_mode(HarnessShell& browser, ShowcaseMode mode);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_MODE_SEED_H_
