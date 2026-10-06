// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LABEL_COMPOSITE_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LABEL_COMPOSITE_H_

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace plugin {
namespace detail {

// DXGI swapchain capture omits GDI overlays. Composite leftover place-names
// onto the showcase BMP so equal-profile inspect matches serial leftovers.
bool composite_legacy_labels_onto_bmp(content::Scene3dPresenter* cam,
                                      const wchar_t* bmp_path,
                                      int w,
                                      int h);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LABEL_COMPOSITE_H_
