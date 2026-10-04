// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_ENCODER_TLS_H_
#define SCENIC_GDI_ENCODER_TLS_H_

#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"

namespace scenic {
namespace detail {

// Thread-local encoder bind kept out of Rhi2dCartoDraw so
// Rhi2dRenderDevice::layer_tree_host_ member offsets stay stable.
inline thread_local Rhi2dCommandEncoder* g_paint_encoder = nullptr;

inline Rhi2dCommandEncoder* active_encoder() { return g_paint_encoder; }

inline void set_paint_encoder(Rhi2dCommandEncoder* encoder) {
  g_paint_encoder = encoder;
}

inline bool paint_encoder_recording() {
  return g_paint_encoder && g_paint_encoder->is_recording();
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_ENCODER_TLS_H_
