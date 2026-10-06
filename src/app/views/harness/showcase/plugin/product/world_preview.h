// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRODUCT_WORLD_PREVIEW_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRODUCT_WORLD_PREVIEW_H_

namespace app {

class Browser;

namespace detail {

// Exercises PluginHost present_dataset(surface=preview) → WorldPreviewView.
int run_world_preview(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRODUCT_WORLD_PREVIEW_H_
