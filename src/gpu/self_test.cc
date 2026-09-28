// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/gpu.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "gpu/display/output_surface.h"

namespace gpu {

int run_self_test(const wchar_t* exe_path) {
  (void)exe_path;
  std::fprintf(stdout, "--type=gpu --self-test: creating D3D device\n");
  detail::OutputSurface present;
  HANDLE self = GetCurrentProcess();
  if (!present.resize(64, 64, content::PresentMode::kSharedTexture, self)) {
    std::fprintf(stderr, "self-test: OutputSurface::resize failed\n");
    return 1;
  }
  if (!present.has_d3d_device()) {
    std::fprintf(stderr, "self-test: no D3D device\n");
    return 2;
  }
  present.clear(0x40, 0x80, 0xC0, 0xFF);
  const content::SharedHandleWire w = present.wire();
  // DXGI CreateSharedHandle can succeed while leaving a null local NT handle on
  // some drivers; DIB fallback still publishes a shareable mapping. Accept either.
  if (w.width_px == 0 || w.height_px == 0) {
    std::fprintf(stderr, "self-test: empty wire size\n");
    return 3;
  }
  if (!present.share_handle() && w.nt_handle == 0) {
    std::fprintf(stderr,
                 "self-test: empty shared handle (local=%p nt=%llu)\n",
                 present.share_handle(),
                 static_cast<unsigned long long>(w.nt_handle));
    return 3;
  }
  std::fprintf(stdout, "self-test ok generation=%u %ux%u d3d=%d share=%p\n",
               w.generation, w.width_px, w.height_px,
               present.has_d3d_device() ? 1 : 0, present.share_handle());
  return 0;
}

}  // namespace gpu
