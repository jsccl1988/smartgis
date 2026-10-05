// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_PRESENT_POINTCLOUD_H_
#define PLUGIN_WORLD3D_SCENE_PRESENT_POINTCLOUD_H_

#include <cstdint>
#include <string>

namespace content {
class GisDocument;
}

namespace plugin {

bool present_world3d_pointcloud(content::GisDocument* doc,
                                const std::string& path);
bool present_world3d_pointcloud_xyz(content::GisDocument* doc,
                                    const float* xyz,
                                    int point_count,
                                    const uint8_t* rgba);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_PRESENT_POINTCLOUD_H_
