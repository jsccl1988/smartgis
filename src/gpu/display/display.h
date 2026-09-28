// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_DISPLAY_DISPLAY_H_
#define GPU_DISPLAY_DISPLAY_H_

// Display: choose direct or tile content, record one CompositorFrame, and
// submit it through FrameComposer on the surface's AdapterId (one present).
// Asynchronous present uses PresentMailbox (BeginFrame + Submit). Callers
// include gpu/frame_sink.h. This header does not re-export raster or
// compositor types.

#include "gpu/frame_sink.h"

#endif  // GPU_DISPLAY_DISPLAY_H_
