// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/backend/paint_backend.h"

#include "legacy/render/rhi2d/impl/gdiplus/backend/gdiplus_backend.h"

namespace render {
namespace detail {

PaintBackend* emplace_paint_backend(void* storage, size_t bytes, HDC hdc) {
  if (!storage || bytes < sizeof(GdiPlusBackend)) {
    return nullptr;
  }
  return new (storage) GdiPlusBackend(hdc);
}

void destroy_paint_backend(PaintBackend* backend) {
  if (backend) {
    backend->~PaintBackend();
  }
}

base::RenderBaseApi rhi2d_port_api() { return base::RD_GDIPLUS; }

const char* rhi2d_port_name() { return "GdiPlus"; }

}  // namespace detail
}  // namespace render
