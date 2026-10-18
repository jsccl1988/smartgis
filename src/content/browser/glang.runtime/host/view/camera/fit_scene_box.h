// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_CAMERA_FIT_SCENE_BOX_H_
#define IL_RUNTIME_BACKEND_VIEW_CAMERA_FIT_SCENE_BOX_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


namespace detail {

// Frame the Scene3D orbit camera from the present world-component AABB
// (local mesh / overlay). Picks distance + pitch for a readable 3/4 view.
// Does not push_shared_extent (plugin pads must not be overwritten by China).
bool fit_scene_box(content::ShellBridge& browser);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_CAMERA_FIT_SCENE_BOX_H_
