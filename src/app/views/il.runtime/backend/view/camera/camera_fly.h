// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_CAMERA_CAMERA_FLY_H_
#define IL_RUNTIME_BACKEND_VIEW_CAMERA_CAMERA_FLY_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// Animate the Scene3D orbit camera after fit_scene_box.
//   mode "orbit"     — yaw arc around the framed world component
//   mode "spherical" — far→near approach with yaw/pitch (globe / skim feel)
// |ms| total wall time; |steps| keyframes (pumps between).
bool camera_fly(Browser& browser, const std::string& mode, int ms, int steps);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_CAMERA_CAMERA_FLY_H_
