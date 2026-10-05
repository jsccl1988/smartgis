// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/present/standin.h"

#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"

namespace plugin {

bool present_world3d_standin_mesh(content::GisDocument* doc,
                                  content::PluginHost::Scene3dSink* sink,
                                  const char* name,
                                  double lon,
                                  double lat,
                                  double half_deg) {
  if (!add_standin_mesh(doc, name, lon, lat, half_deg)) {
    return false;
  }
  if (sink) {
    (void)sink->add_standin_mesh(name ? name : "", lon, lat, half_deg);
    sink->invalidate();
  }
  return true;
}

}  // namespace plugin
