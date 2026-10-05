// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/present/style.h"

#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"

namespace plugin {

bool apply_world3d_mesh_style(content::GisDocument* doc) {
  return apply_style_resource(doc, "smartgis.world3d", "world3d.style.json");
}

}  // namespace plugin
