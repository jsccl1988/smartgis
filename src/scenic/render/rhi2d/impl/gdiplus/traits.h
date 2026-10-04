// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_GDIPLUS_TRAITS_H_
#define SCENIC_RHI2D_IMPL_GDIPLUS_TRAITS_H_

#include "scenic/render/rhi2d/public/device/viewport.h"
#include "scenic/render/rhi2d/impl/gdiplus/backend/gdiplus_backend.h"

namespace scenic {
namespace detail {

struct GdiPlusBackendTraits {
  using Backend = GdiPlusBackend;
  static constexpr base::RenderBaseApi k_api = base::RD_GDIPLUS;
  static constexpr const char* k_name = "GdiPlus";
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_GDIPLUS_TRAITS_H_
