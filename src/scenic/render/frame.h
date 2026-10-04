// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_FRAME_H_
#define SCENIC_FRAME_H_

// Copy-backend frame boundary helpers (rhi2d/rhi3d/scene3d). Hosts and the
// product façade must not include this header. Not compiled into scenic.dll.
#include "base/core/log.h"
#include "base/memory/sample_trace.h"

namespace scenic {
namespace detail {

// Emit Chrome Trace Memory counters when process_trace is armed. Call once
// per completed Scenic frame (GDI paint_once, rhi3d SwapBuffers, scene
// Render) — cheap no-op when tracing is off.
inline void finish_frame_memory_sample() {
  base::sample_memory_counters_to_process_trace();
}

// Sparse frame / stage log for Debug Console (log_sink). Not per-feature.
inline void log_frame_flow(const char* msg) {
  if (!msg || !msg[0]) {
    return;
  }
  LOGGING(LOG_INFO, "[scenic.flow] %s", msg);
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_FRAME_H_
