// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_PAINT_ENCODER_TLS_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_PAINT_ENCODER_TLS_H_

#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"

namespace render {
namespace detail {

// Thread-local encoder bind kept out of GdiPaintCanvas so
// SmtGdiRenderDevice::render_thread_ member offsets stay stable.
inline thread_local GdiCommandEncoder* g_paint_encoder = nullptr;

inline GdiCommandEncoder* active_encoder() { return g_paint_encoder; }

inline void set_paint_encoder(GdiCommandEncoder* encoder) {
  g_paint_encoder = encoder;
}

inline bool paint_encoder_recording() {
  return g_paint_encoder && g_paint_encoder->is_recording();
}

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_PAINT_ENCODER_TLS_H_
