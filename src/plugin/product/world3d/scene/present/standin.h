// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_PRESENT_STANDIN_H_
#define PLUGIN_WORLD3D_SCENE_PRESENT_STANDIN_H_

namespace content {
class GisDocument;
}

namespace plugin {
class Scene3dSink;

// Tiny triangle on GisDocument plus Scene3dSink stand-in (if bridged).
bool present_world3d_standin_mesh(content::GisDocument* doc,
                                  Scene3dSink* sink,
                                  const char* name,
                                  double lon,
                                  double lat,
                                  double half_deg);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_PRESENT_STANDIN_H_
