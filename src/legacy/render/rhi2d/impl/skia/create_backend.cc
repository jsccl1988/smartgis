// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/backend/paint_backend.h"

#include "legacy/render/rhi2d/impl/skia/backend/skia_backend.h"

namespace render {
namespace detail {

PaintBackend* emplace_paint_backend(void* storage, size_t bytes, HDC hdc) {
  if (!storage || bytes < sizeof(SkiaBackend)) {
    return nullptr;
  }
  return new (storage) SkiaBackend(hdc);
}

void destroy_paint_backend(PaintBackend* backend) {
  if (backend) {
    backend->~PaintBackend();
  }
}

base::RenderBaseApi rhi2d_port_api() { return base::RD_SKIA; }

const char* rhi2d_port_name() { return "Skia"; }

}  // namespace detail
}  // namespace render
