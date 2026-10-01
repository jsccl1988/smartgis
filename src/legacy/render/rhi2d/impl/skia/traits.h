// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_SKIA_TRAITS_H_
#define LEGACY_RENDER_RHI2D_IMPL_SKIA_TRAITS_H_

#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/skia/backend/skia_backend.h"

namespace render {
namespace detail {

struct SkiaBackendTraits {
  using Backend = SkiaBackend;
  static constexpr base::RenderBaseApi k_api = base::RD_SKIA;
  static constexpr const char* k_name = "Skia";
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_SKIA_TRAITS_H_
