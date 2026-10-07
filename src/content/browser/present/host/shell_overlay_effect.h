// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_
#define CONTENT_BROWSER_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_

// Compatibility alias: implementation lives in render::graph (render.dll).
// Map2d / Scene3d present hosts still include this path.
#include "render/graph/shell_overlay_effect.h"

namespace content {
namespace detail {

using ShellOverlayEffect = render::graph::ShellOverlayEffect;

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_
