// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_APP_RENDERER_MAIN_H
#define CONTENT_APP_RENDERER_MAIN_H

#include "content/app/content_main.h"

namespace content {

// --type=renderer: SmtMap / tools (CPU). Must not create a D3D/GL device.
int RendererMain(const ContentMainParams& params);

}  // namespace content

#endif  // CONTENT_APP_RENDERER_MAIN_H
