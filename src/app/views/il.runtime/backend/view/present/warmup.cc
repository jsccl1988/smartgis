// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/present/warmup.h"

#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"

#include <cstdio>

namespace app {
namespace detail {

int present_gpu_warmup(content::Scene3dPresenter* cam,
                       render::rhi::Device* device,
                       const PresentGpuWarmupOpts& opts) {
  if (!cam || !device) {
    return 52;
  }
  const int frames = opts.frames;
  if (frames <= 0) {
    return 0;
  }
  for (int i = 0; i < frames; ++i) {
    if (opts.numbered_frame_marks) {
      char frame_mark[32];
      std::snprintf(frame_mark, sizeof(frame_mark), "present-%d", i);
      mark_step(opts.mark, frame_mark);
    } else {
      mark_step(opts.mark, opts.frame_mark);
    }
    if (!present_scene3d_gpu(cam, device, opts.width_px, opts.height_px)) {
      if (opts.fail_log_prefix) {
        std::fprintf(stderr, "%s: present_gpu failed frame %d\n",
                     opts.fail_log_prefix, i);
      }
      if (opts.on_fail) {
        opts.on_fail(i, opts.on_fail_user);
      }
      return 52;
    }
    if (opts.pump_ms > 0) {
      pump_messages(static_cast<DWORD>(opts.pump_ms));
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace app
