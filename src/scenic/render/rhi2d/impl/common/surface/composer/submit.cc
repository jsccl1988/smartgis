// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/surface/composer/submit.h"

#include <mutex>

namespace scenic {
namespace detail {

std::mutex g_submit_lock;
Rhi2dCompositorSubmitFn g_submit_fn;
Rhi2dBgraSubmitFn g_bgra_fn = nullptr;
void* g_bgra_user = nullptr;

void set_compositor_submit(Rhi2dCompositorSubmitFn fn) {
  std::lock_guard<std::mutex> lock(g_submit_lock);
  g_submit_fn = std::move(fn);
}

void clear_compositor_submit() {
  std::lock_guard<std::mutex> lock(g_submit_lock);
  g_submit_fn = nullptr;
}

bool has_compositor_submit() {
  std::lock_guard<std::mutex> lock(g_submit_lock);
  return static_cast<bool>(g_submit_fn) || g_bgra_fn != nullptr;
}

CompositorSubmitSinks snapshot_compositor_submit() {
  std::lock_guard<std::mutex> lock(g_submit_lock);
  CompositorSubmitSinks out;
  out.frame_fn = g_submit_fn;
  out.bgra_fn = g_bgra_fn;
  out.bgra_user = g_bgra_user;
  return out;
}

}  // namespace detail
}  // namespace scenic

extern "C" {

void Rhi2dSetBgraSubmit(Rhi2dBgraSubmitFn fn, void* user) {
  std::lock_guard<std::mutex> lock(scenic::detail::g_submit_lock);
  scenic::detail::g_bgra_fn = fn;
  scenic::detail::g_bgra_user = user;
}

void Rhi2dClearBgraSubmit(void) {
  std::lock_guard<std::mutex> lock(scenic::detail::g_submit_lock);
  scenic::detail::g_bgra_fn = nullptr;
  scenic::detail::g_bgra_user = nullptr;
}

}  // extern "C"
