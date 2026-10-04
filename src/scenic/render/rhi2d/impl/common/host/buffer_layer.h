// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SCENIC_RENDER_RHI2D_IMPL_COMMON_HOST_BUFFER_LAYER_H_
#define SCENIC_RENDER_RHI2D_IMPL_COMMON_HOST_BUFFER_LAYER_H_

namespace scenic {
namespace detail {

// Impl-only compose targets. Not part of rhi2d/public.
enum eRDBufferLayer { MRD_BL_MAP, MRD_BL_DYNAMIC, MRD_BL_QUICK, MRD_BL_DIRECT };

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RENDER_RHI2D_IMPL_COMMON_HOST_BUFFER_LAYER_H_
