// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_LABEL_COMPOSITE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_LABEL_COMPOSITE_H_

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {
namespace detail {

// DXGI swapchain capture omits GDI overlays. Composite leftover place-names
// onto the showcase BMP so equal-profile inspect matches serial leftovers.
bool composite_legacy_labels_onto_bmp(content::Scene3dPresenter* cam,
                                      const wchar_t* bmp_path,
                                      int w,
                                      int h);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_LABEL_COMPOSITE_H_
