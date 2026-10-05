// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_
#define SCENIC_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_

#include "scenic/render/rhi3d/impl/d3d/prerequisites.h"

namespace scenic {
namespace detail {

class D3dRenderDevice;

// Env RHI3D_D3D_DEFERRED: unset/1/y → on; 0/n/f → off.
bool d3d_deferred_env_enabled();

// Per-device deferred-context pool (P3). Workers record; FrameJob executes.
struct D3dDeferredSlot {
  ID3D11DeviceContext* ctx = nullptr;
  ID3D11CommandList* list = nullptr;
  ID3D11Buffer* mesh_cb = nullptr;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_D3D_HOST_DEFERRED_DRAW_H_
