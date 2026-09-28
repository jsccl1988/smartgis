// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/compositor/frame_composer.h"

#include "gpu/compositor/rhi_composer.h"
#include "gpu/compositor/software_renderer.h"
#include "gpu/device/gpu_device_hub.h"
#include "render/rhi/rhi.h"

#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gpu {
namespace detail {
namespace {

bool g_has_override = false;
ComposeBackend g_override = ComposeBackend::kSoftware;
bool g_logged_rhi_fallback = false;

ComposeBackend env_compose_backend() {
  char buf[32] = {};
  const DWORD n =
      GetEnvironmentVariableA("SMT_GPU_COMPOSE", buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) {
    return ComposeBackend::kSoftware;
  }
  for (char* p = buf; *p; ++p) {
    if (*p >= 'A' && *p <= 'Z') {
      *p = static_cast<char>(*p - 'A' + 'a');
    }
  }
  if (std::strcmp(buf, "rhi") == 0) {
    return ComposeBackend::kRhi;
  }
  if (std::strcmp(buf, "software") == 0) {
    return ComposeBackend::kSoftware;
  }
  return ComposeBackend::kSoftware;
}

}  // namespace

ComposeBackend select_compose_backend() {
  if (g_has_override) {
    return g_override;
  }
  return env_compose_backend();
}

void set_compose_backend(ComposeBackend backend) {
  g_has_override = true;
  g_override = backend;
}

void clear_compose_backend_override() {
  g_has_override = false;
}

std::unique_ptr<FrameComposer> make_frame_composer(ComposeBackend backend,
                                                   AdapterId adapter) {
  const AdapterId id =
      (adapter == kAdapterInvalid) ? kAdapterPrimary : adapter;
  GpuDeviceHub& hub = device_hub();

  if (backend == ComposeBackend::kRhi) {
    if (hub.is_software_fallback(id)) {
      return std::make_unique<SoftwareComposer>(id);
    }
    // Eagerly create the per-adapter RHI device so RhiComposer can compose.
    render::rhi::Device* device = hub.ensure_rhi_device(id);
    if (!device) {
      if (!g_logged_rhi_fallback) {
        g_logged_rhi_fallback = true;
        std::fprintf(stderr,
                     "gpu: SMT_GPU_COMPOSE=rhi — RHI device init failed for "
                     "adapter %u; sticky software fallback (GPU process)\n",
                     static_cast<unsigned>(id));
      }
      return std::make_unique<SoftwareComposer>(id);
    }
    return std::make_unique<RhiComposer>(id);
  }

  return std::make_unique<SoftwareComposer>(id);
}

}  // namespace detail
}  // namespace gpu
