// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/gpu.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "gpu/present.h"

namespace gpu {

int run_self_test(const wchar_t* exe_path) {
  (void)exe_path;
  std::fprintf(stdout, "--type=gpu --self-test: creating D3D device\n");
  detail::PresentTarget present;
  HANDLE self = GetCurrentProcess();
  if (!present.resize(64, 64, content::PresentMode::kSharedTexture, self)) {
    std::fprintf(stderr, "self-test: PresentTarget::resize failed\n");
    return 1;
  }
  if (!present.has_d3d_device()) {
    std::fprintf(stderr, "self-test: no D3D device\n");
    return 2;
  }
  present.paint_clear(0x40, 0x80, 0xC0, 0xFF);
  const content::SharedHandleWire w = present.wire();
  if (!w.nt_handle || w.width_px == 0 || w.height_px == 0) {
    std::fprintf(stderr, "self-test: empty shared handle\n");
    return 3;
  }
  std::fprintf(stdout, "self-test ok generation=%u %ux%u d3d=1\n",
               w.generation, w.width_px, w.height_px);
  return 0;
}

}  // namespace gpu
