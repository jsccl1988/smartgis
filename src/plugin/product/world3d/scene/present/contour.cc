// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/present/contour.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"

namespace plugin {

bool present_world3d_contour_suite(content::Scene3dPresenter* scene3d) {
  if (!scene3d) {
    return false;
  }
  return scene3d->atmosphere_session().apply_contour_suite_defaults();
}

}  // namespace plugin
