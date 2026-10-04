// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_RENDERER_RENDERER_MAIN_H
#define CONTENT_RENDERER_RENDERER_MAIN_H

#include "content/app/content_main.h"
#include "content/content_export.h"

namespace content {

// --type=renderer: Map / tools (CPU). Must not create a D3D/GL device.
CONTENT_EXPORT int RendererMain(const ContentMainParams& params);

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_MAIN_H
