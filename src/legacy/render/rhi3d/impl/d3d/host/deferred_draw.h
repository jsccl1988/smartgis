// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_
#define LEGACY_RENDER_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"

namespace render {

class SmtD3DRenderDevice;

// Env SMT_RHI3D_D3D_DEFERRED: unset/1/y → on; 0/n/f → off.
bool d3d_deferred_env_enabled();

// Per-device deferred-context pool (P3). Workers record; FrameJob executes.
struct D3dDeferredSlot {
  ID3D11DeviceContext* ctx = nullptr;
  ID3D11CommandList* list = nullptr;
  ID3D11Buffer* mesh_cb = nullptr;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_
