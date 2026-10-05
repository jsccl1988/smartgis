// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_PRESENT_MASK_H_
#define PLUGIN_STORMSURGE_PRESENT_MASK_H_

#include "content/public/plugin_host.h"

namespace content {
class GisDocument;
class Scene3dPresenter;
}

namespace plugin {

// Wet-cell polygons on GisDocument. Optional Scene3dPresenter overlay clear
// is chrome-only (sink has no TIN API).
bool present_stormsurge_mask(content::GisDocument* doc,
                             content::PluginHost::Scene3dSink* sink,
                             content::Scene3dPresenter* scene3d,
                             const unsigned char* mask,
                             int width,
                             int height,
                             const double* geotransform,
                             bool begin_session,
                             double water_level);

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_PRESENT_MASK_H_
