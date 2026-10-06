// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MAP2D_EXPORT_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MAP2D_EXPORT_H_

#include "content/public/map_layer_types.h"

namespace app {

class Browser;

namespace detail {

// Frame Map2d (optional extent) and export_bmp under the exe capture dir.
bool try_export_map2d_bmp(Browser& browser, const char* leaf_utf8,
                          const content::Extent2* extent_or_null);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_MAP2D_EXPORT_H_
