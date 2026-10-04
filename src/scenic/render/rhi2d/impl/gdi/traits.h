// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_GDI_TRAITS_H_
#define SCENIC_RHI2D_IMPL_GDI_TRAITS_H_

#include "scenic/detail/viewport.h"
#include "scenic/render/rhi2d/impl/gdi/backend/gdi_backend.h"

namespace scenic {
namespace detail {

struct GdiBackendTraits {
  using Backend = GdiBackend;
  static constexpr base::RenderBaseApi k_api = base::RD_GDI;
  static constexpr const char* k_name = "Gdi";
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_GDI_TRAITS_H_
