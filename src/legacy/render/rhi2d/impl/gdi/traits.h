// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_TRAITS_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_TRAITS_H_

#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/gdi/backend/gdi_backend.h"

namespace render {
namespace detail {

struct GdiBackendTraits {
  using Backend = GdiBackend;
  static constexpr base::RenderBaseApi k_api = base::RD_GDI;
  static constexpr const char* k_name = "Gdi";
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_TRAITS_H_
