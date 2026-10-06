// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD_PREVIEW_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD_PREVIEW_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Exercises PluginHost present_dataset(surface=preview) → WorldPreviewView.
int run_world_preview(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD_PREVIEW_H_
