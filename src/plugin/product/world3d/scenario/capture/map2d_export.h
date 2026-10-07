// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_CAPTURE_MAP2D_EXPORT_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_CAPTURE_MAP2D_EXPORT_H_

#include "content/public/types.h"

namespace plugin {

class HarnessShell;

namespace detail {

// Frame Map2d (optional extent) and export_bmp under the exe capture dir.
bool try_export_map2d_bmp(HarnessShell& browser, const char* leaf_utf8,
                          const content::Extent2* extent_or_null);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_CAPTURE_MAP2D_EXPORT_H_
