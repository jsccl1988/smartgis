// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_DETAIL_FRAME_PIPELINE_H_
#define LEGACY_RENDER_DETAIL_FRAME_PIPELINE_H_

// scenic::detail helper for copy backends (rhi2d/rhi3d/scene3d). Hosts and
// the product façade must not include this header. Not compiled into scenic.dll.
#include "base/core/log.h"
#include "base/memory/sample_trace.h"

namespace scenic {
namespace detail {

// Emit Chrome Trace Memory counters when process_trace is armed. Call once
// per completed leftover frame (GDI paint_once, rhi3d SwapBuffers, scene
// Render) — cheap no-op when tracing is off.
inline void finish_legacy_frame_memory_sample() {
  base::sample_memory_counters_to_process_trace();
}

// Key leftover flow line for Debug Console (log_sink). Keep sparse — frame /
// stage boundaries only, not per-feature.
inline void log_legacy_flow(const char* msg) {
  if (!msg || !msg[0]) {
    return;
  }
  LOGGING(LOG_INFO, "[legacy.flow] %s", msg);
}

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_DETAIL_FRAME_PIPELINE_H_
